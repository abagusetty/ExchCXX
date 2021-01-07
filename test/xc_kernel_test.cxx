#include "ut_common.hpp"

using namespace ExchCXX;

TEST_CASE( "XCKernel Metadata Validity", "[xc-kernel]" ) {

  const int npts = 1024;

  auto lda_kernel_test = Kernel::SlaterExchange;
  auto gga_kernel_test = Kernel::LYP;
  auto hyb_kernel_test = Kernel::B3LYP;

  Backend backend;

  SECTION( "Pure LDA Unpolarized" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    XCKernel lda( backend, lda_kernel_test, Spin::Unpolarized );

    CHECK( lda.is_lda() );
    CHECK( not lda.is_polarized() );
    CHECK( not lda.is_gga() );
    CHECK( not lda.is_mgga() );
    CHECK( not lda.is_hyb() );

    CHECK( lda.rho_buffer_len( npts )    == npts );
    CHECK( lda.sigma_buffer_len( npts )  == 0    );
    CHECK( lda.exc_buffer_len( npts )    == npts );
    CHECK( lda.vrho_buffer_len( npts )   == npts );
    CHECK( lda.vsigma_buffer_len( npts ) == 0    );

  }

  SECTION( "Pure LDA Polarized" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    XCKernel lda( backend, lda_kernel_test, Spin::Polarized );

    CHECK( lda.is_lda() );
    CHECK( lda.is_polarized() );
    CHECK( not lda.is_gga() );
    CHECK( not lda.is_mgga() );
    CHECK( not lda.is_hyb() );

    CHECK( lda.rho_buffer_len( npts )    == 2*npts );
    CHECK( lda.sigma_buffer_len( npts )  == 0      );
    CHECK( lda.exc_buffer_len( npts )    == npts   );
    CHECK( lda.vrho_buffer_len( npts )   == 2*npts );
    CHECK( lda.vsigma_buffer_len( npts ) == 0      );

  }


  SECTION( "Pure GGA Unpolarized" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    XCKernel gga( backend, gga_kernel_test, Spin::Unpolarized );

    CHECK( gga.is_gga() );
    CHECK( not gga.is_polarized() );
    CHECK( not gga.is_lda() );
    CHECK( not gga.is_mgga() );
    CHECK( not gga.is_hyb() );

    CHECK( gga.rho_buffer_len( npts )    == npts );
    CHECK( gga.sigma_buffer_len( npts )  == npts );
    CHECK( gga.exc_buffer_len( npts )    == npts );
    CHECK( gga.vrho_buffer_len( npts )   == npts );
    CHECK( gga.vsigma_buffer_len( npts ) == npts );

  }

  SECTION( "Pure GGA Polarized" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    XCKernel gga( backend, gga_kernel_test, Spin::Polarized );

    CHECK( gga.is_gga() );
    CHECK( gga.is_polarized() );
    CHECK( not gga.is_lda() );
    CHECK( not gga.is_mgga() );
    CHECK( not gga.is_hyb() );

    CHECK( gga.rho_buffer_len( npts )    == 2*npts );
    CHECK( gga.sigma_buffer_len( npts )  == 3*npts );
    CHECK( gga.exc_buffer_len( npts )    == npts   );
    CHECK( gga.vrho_buffer_len( npts )   == 2*npts );
    CHECK( gga.vsigma_buffer_len( npts ) == 3*npts );

  }

  SECTION( "Hybrid" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    XCKernel hyb( backend, hyb_kernel_test, Spin::Unpolarized );
    CHECK( hyb.is_hyb() );

  }

}

TEST_CASE( "XCKernel Metadata Correctness", "[xc-kernel]" ) {

  Backend backend;
  SECTION( "LDA Kernels" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    for( const auto& kern : lda_kernels ) {
      XCKernel func( backend, kern, Spin::Unpolarized );
      auto exx = load_reference_exx( kern );

      CHECK( func.is_lda() );
      CHECK( exx == Approx( func.hyb_exx() ) );

      if( std::abs(exx) > 0 ) CHECK( func.is_hyb() );
    }

  }

  SECTION( "GGA Kernels" ) {

    SECTION( "Libxc Backend" )   { backend = Backend::libxc; }
    SECTION( "Builtin Backend" ) { backend = Backend::builtin; }

    for( const auto& kern : gga_kernels ) {
      XCKernel func( backend, kern, Spin::Unpolarized );
      auto exx = load_reference_exx( kern );

      CHECK( func.is_gga() );
      CHECK( exx == Approx( func.hyb_exx() ) );

      if( std::abs(exx) > 0 ) CHECK( func.is_hyb() );
    }

  }

}








void kernel_test( TestInterface interface, Backend backend, Kernel kern, 
  Spin polar ) {

  // Create the kernel
  XCKernel func( backend, kern, polar );

  const double alpha = 3.14;
  const double fill_val_e = 2.;
  const double fill_val_vr = 10.;
  const double fill_val_vs = 50.;

  const bool use_ref_values = 
    (interface != TestInterface::EXC_INC) and
    (interface != TestInterface::EXC_VXC_INC);

  // LDA XC Kernels
  if( func.is_lda() ) {

    // Get reference values
    auto ref_vals = use_ref_values ? 
      load_lda_reference_values( kern, polar ) :
      gen_lda_reference_values( backend,kern, polar );

    const auto& [npts, rho, exc_ref, vrho_ref] = ref_vals;

    // Allocate buffers
    std::vector<double> exc( func.exc_buffer_len( npts ), fill_val_e );
    std::vector<double> vrho( func.vrho_buffer_len( npts ), fill_val_vr );

    if( interface == TestInterface::EXC ) {

      // Evaluate XC kernel
      func.eval_exc( npts, rho.data(), exc.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(exc_ref[i]) );

    }

    if( interface == TestInterface::EXC_INC ) {

      // Evaluate XC kernel
      func.eval_exc_inc( alpha, npts, rho.data(), exc.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(fill_val_e + alpha * exc_ref[i]) );

    }

    if( interface == TestInterface::EXC_VXC ) {

      // Evaluate XC kernel
      func.eval_exc_vxc( npts, rho.data(), exc.data(), vrho.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(exc_ref[i]) );
      for( auto i = 0ul; i < func.vrho_buffer_len(npts); ++i )
        CHECK( vrho[i] == Approx(vrho_ref[i]) );

    }

    if( interface == TestInterface::EXC_VXC_INC ) {

      // Evaluate XC kernel
      func.eval_exc_vxc_inc( alpha, npts, rho.data(), exc.data(), vrho.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(fill_val_e + alpha * exc_ref[i]) );
      for( auto i = 0ul; i < func.vrho_buffer_len(npts); ++i )
        CHECK( vrho[i] == Approx(fill_val_vr + alpha * vrho_ref[i]) );

    }

  // GGA XC Kernels
  } else if( func.is_gga() ) {

    // Get reference values
    auto ref_vals = use_ref_values ? 
      load_gga_reference_values( kern, polar ) :
      gen_gga_reference_values( backend,kern, polar );

    const auto& [npts, rho, sigma, exc_ref, vrho_ref, vsigma_ref] = ref_vals;

    // Allocate buffers
    std::vector<double> exc( func.exc_buffer_len( npts ), fill_val_e );
    std::vector<double> vrho( func.vrho_buffer_len( npts ), fill_val_vr );
    std::vector<double> vsigma( func.vsigma_buffer_len( npts ), fill_val_vs );

    if( interface == TestInterface::EXC ) {

      // Evaluate XC kernel
      func.eval_exc( npts, rho.data(), sigma.data(), exc.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(exc_ref[i]) );

    }

    if( interface == TestInterface::EXC_INC ) {

      // Evaluate XC kernel
      func.eval_exc_inc( alpha, npts, rho.data(), sigma.data(), exc.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(fill_val_e + alpha * exc_ref[i]) );

    }

    if( interface == TestInterface::EXC_VXC ) {

      // Evaluate XC kernel
      func.eval_exc_vxc( npts, rho.data(), sigma.data(), exc.data(), 
        vrho.data(), vsigma.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(exc_ref[i]) );
      for( auto i = 0ul; i < func.vrho_buffer_len(npts); ++i )
        CHECK( vrho[i] == Approx(vrho_ref[i]) );
      for( auto i = 0ul; i < func.vsigma_buffer_len(npts); ++i )
        CHECK( vsigma[i] == Approx(vsigma_ref[i]) );

    }
    
    if( interface == TestInterface::EXC_VXC_INC ) {

      // Evaluate XC kernel
      func.eval_exc_vxc_inc( alpha, npts, rho.data(), sigma.data(), exc.data(), 
        vrho.data(), vsigma.data() );

      // Check correctness
      for( auto i = 0ul; i < func.exc_buffer_len(npts); ++i )
        CHECK( exc[i] == Approx(fill_val_e + alpha * exc_ref[i]) );
      for( auto i = 0ul; i < func.vrho_buffer_len(npts); ++i )
        CHECK( vrho[i] == Approx(fill_val_vr + alpha * vrho_ref[i]) );
      for( auto i = 0ul; i < func.vsigma_buffer_len(npts); ++i )
        CHECK( vsigma[i] == Approx(fill_val_vs + alpha * vsigma_ref[i]) );

    }

  }

}

TEST_CASE( "Libxc Correctness Check", "[xc-libxc]" ) {

  SECTION( "SlaterExchange Unpolarized: EXC" ) {
    kernel_test( TestInterface::EXC, Backend::libxc, Kernel::SlaterExchange, 
      Spin::Unpolarized );
  }
  SECTION( "SlaterExchange Unpolarized: EXC + VXC" ) {
    kernel_test( TestInterface::EXC_VXC, Backend::libxc, Kernel::SlaterExchange, 
      Spin::Unpolarized );
  }

  SECTION( "SlaterExchange Polarized: EXC" ) {
    kernel_test( TestInterface::EXC, Backend::libxc, Kernel::SlaterExchange, 
      Spin::Polarized );
  }
  SECTION( "SlaterExchange Polarized: EXC + VXC" ) {
    kernel_test( TestInterface::EXC_VXC, Backend::libxc, Kernel::SlaterExchange, 
      Spin::Polarized );
  }

  SECTION( "LYP Unpolarized: EXC" ) {
    kernel_test( TestInterface::EXC, Backend::libxc, Kernel::LYP, 
      Spin::Unpolarized );
  }
  SECTION( "LYP Unpolarized: EXC + VXC" ) {
    kernel_test( TestInterface::EXC_VXC, Backend::libxc, Kernel::LYP, 
      Spin::Unpolarized );
  }

  SECTION( "LYP Polarized: EXC" ) {
    kernel_test( TestInterface::EXC, Backend::libxc, Kernel::LYP, 
      Spin::Polarized );
  }
  SECTION( "LYP Polarized: EXC + VXC" ) {
    kernel_test( TestInterface::EXC_VXC, Backend::libxc, Kernel::LYP, 
      Spin::Polarized );
  }

}


void compare_libxc_builtin( TestInterface interface, EvalType evaltype, 
  Kernel kern, Spin polar ) {

  auto [npts_lda, ref_rho]   = load_reference_density( polar );
  auto [npts_gga, ref_sigma] = load_reference_sigma  ( polar );

  REQUIRE( npts_lda == npts_gga );

  const int npts = npts_lda;

  XCKernel func_libxc  ( Backend::libxc,   kern, polar );
  XCKernel func_builtin( Backend::builtin, kern, polar );

  const int len_rho   = func_libxc.rho_buffer_len( npts );
  const int len_sigma = func_libxc.sigma_buffer_len( npts );

  std::vector<double> rho_small(len_rho, 1e-13);
  std::vector<double> sigma_small(len_sigma, 1e-14);

  std::vector<double> rho_zero(len_rho, 0.);
  std::vector<double> sigma_zero(len_sigma, 0.);

  std::vector<double> rho_use, sigma_use;

  if( evaltype == EvalType::Regular ) {
    rho_use   = ref_rho;
    sigma_use = ref_sigma;
  }
 
  if( evaltype == EvalType::Small ) {
    rho_use   = rho_small;
    sigma_use = sigma_small;
  }
 
  if( evaltype == EvalType::Zero ) {
    rho_use   = rho_zero;
    sigma_use = sigma_zero;
  }



  std::vector<double> exc_libxc( func_builtin.exc_buffer_len(npts) );
  std::vector<double> vrho_libxc( func_builtin.vrho_buffer_len(npts) );
  std::vector<double> vsigma_libxc( func_builtin.vsigma_buffer_len(npts) );

  std::vector<double> exc_builtin( func_builtin.exc_buffer_len(npts) );
  std::vector<double> vrho_builtin( func_builtin.vrho_buffer_len(npts) );
  std::vector<double> vsigma_builtin( func_builtin.vsigma_buffer_len(npts) );

  if( func_libxc.is_lda() ) {

    if( interface == TestInterface::EXC ) {

      func_libxc.eval_exc( npts, rho_use.data(), exc_libxc.data() );
      func_builtin.eval_exc( npts, rho_use.data(), exc_builtin.data() );

    } else if( interface == TestInterface::EXC_VXC ) {

      func_libxc.eval_exc_vxc( npts, rho_use.data(), exc_libxc.data(),
        vrho_libxc.data() );        
      func_builtin.eval_exc_vxc( npts, rho_use.data(), exc_builtin.data(),
        vrho_builtin.data() );        

    }

  } else if( func_libxc.is_gga() ) {

    if( interface == TestInterface::EXC ) {

      func_libxc.eval_exc( npts, rho_use.data(), sigma_use.data(),
        exc_libxc.data() );        
      func_builtin.eval_exc( npts, rho_use.data(), sigma_use.data(),
        exc_builtin.data() );        

    } else if( interface == TestInterface::EXC_VXC ) {

      func_libxc.eval_exc_vxc( npts, rho_use.data(), sigma_use.data(),
        exc_libxc.data(), vrho_libxc.data(), vsigma_libxc.data() );        
      func_builtin.eval_exc_vxc( npts, rho_use.data(), sigma_use.data(),
        exc_builtin.data(), vrho_builtin.data(), vsigma_builtin.data() );        

    }

  }

  // Check correctness
  for( auto i = 0ul; i < func_libxc.exc_buffer_len(npts); ++i ) {
    INFO( "EXC Fails: Kernel is " << kern );
    CHECK( exc_builtin[i] == Approx(exc_libxc[i]) );
  }

  if( interface == TestInterface::EXC_VXC ) {
    for( auto i = 0ul; i < func_libxc.vrho_buffer_len(npts); ++i ) {
      INFO( "VRHO Fails: Kernel is " << kern );
      CHECK( vrho_builtin[i] == Approx(vrho_libxc[i]) );
    }
    for( auto i = 0ul; i < func_libxc.vsigma_buffer_len(npts); ++i ) {
      INFO( "VSIGMA Fails: Kernel is " << kern );
      CHECK( vsigma_builtin[i] == Approx(vsigma_libxc[i]) );
    }
  }

}

TEST_CASE( "Builtin Corectness Test", "[xc-builtin]" ) { 

  SECTION( "Unpolarized Regular Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Regular,
        kern, Spin::Unpolarized );
  }

  SECTION( "Unpolarized Regular Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Regular,
        kern, Spin::Unpolarized );
  }

  SECTION( "Unpolarized Small Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Small,
        kern, Spin::Unpolarized );
  }

  SECTION( "Unpolarized Small Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Small,
        kern, Spin::Unpolarized );
  }

  SECTION( "Unpolarized Zero Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Zero,
        kern, Spin::Unpolarized );
  }

  SECTION( "Unpolarized Zero Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Zero,
        kern, Spin::Unpolarized );
  }






  SECTION( "Polarized Regular Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Regular,
        kern, Spin::Polarized );
  }

  SECTION( "Polarized Regular Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Regular,
        kern, Spin::Polarized );
  }

  SECTION( "Polarized Small Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Small,
        kern, Spin::Polarized );
  }

  SECTION( "Polarized Small Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Small,
        kern, Spin::Polarized );
  }

  SECTION( "Polarized Zero Eval : EXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC, EvalType::Zero,
        kern, Spin::Polarized );
  }

  SECTION( "Polarized Zero Eval : EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      compare_libxc_builtin( TestInterface::EXC_VXC, EvalType::Zero,
        kern, Spin::Polarized );
  }

}


TEST_CASE( "Scale and Increment Interface", "[xc-inc]" ) { 

  SECTION( "Builtin Unpolarized EXC" ) {
    for( auto kern : builtin_supported_kernels )
      kernel_test( TestInterface::EXC_INC, Backend::builtin, kern,
        Spin::Unpolarized );
  }

  SECTION( "Builtin Unpolarized EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      kernel_test( TestInterface::EXC_VXC_INC, Backend::builtin, kern,
        Spin::Unpolarized );
  }

  SECTION( "Builtin Polarized EXC" ) {
    for( auto kern : builtin_supported_kernels )
      kernel_test( TestInterface::EXC_INC, Backend::builtin, kern,
        Spin::Polarized );
  }

  SECTION( "Builtin Polarized EXC + VXC" ) {
    for( auto kern : builtin_supported_kernels )
      kernel_test( TestInterface::EXC_VXC_INC, Backend::builtin, kern,
        Spin::Polarized );
  }
}

TEST_CASE( "kernel_map Test", "[xc-kernel-map]") {

  SECTION("Conversion of String to Kernel") {

    for (auto pair : string_kernal_pairs) {
      CHECK(kernel_map.value(pair.first) == pair.second);
    }

  }

  SECTION("Conversion of Kernel to String") {

    for (auto pair : string_kernal_pairs) {
      CHECK(kernel_map.key(pair.second) == pair.first);
    }

  }

}

#ifdef EXCHCXX_ENABLE_CUDA

template <typename T>
T* safe_cuda_malloc( size_t n ) {

  T* ptr = nullptr;;
  if( n ) {
    auto stat = cudaMalloc( (void**)&ptr, n*sizeof(T) );
    if( stat != cudaSuccess )
      throw std::runtime_error(cudaGetErrorString( stat ));
  }
  return ptr;

}

template <typename T>
void safe_cuda_cpy( T* dest, const T* src, size_t len ) {

  auto stat = cudaMemcpy( dest, src, len*sizeof(T), cudaMemcpyDefault );
  if( stat != cudaSuccess )
    throw std::runtime_error(cudaGetErrorString( stat ));

}

void cuda_free_all(){ }
template <typename T, typename... Args>
void cuda_free_all( T* ptr, Args&&... args ) {

  if( ptr ) {
    auto stat = cudaFree( ptr );
    if( stat != cudaSuccess )
      throw std::runtime_error(cudaGetErrorString( stat ));
  }

  cuda_free_all( std::forward<Args>(args)... );


}

void device_synchronize() {
  auto stat = cudaDeviceSynchronize();
  if( stat != cudaSuccess )
    throw std::runtime_error(cudaGetErrorString( stat ));
}


void test_cuda_interface( TestInterface interface, EvalType evaltype,
  Backend backend, Kernel kern, Spin polar ) {

  auto [npts_lda, ref_rho]   = load_reference_density( polar );
  auto [npts_gga, ref_sigma] = load_reference_sigma  ( polar );

  REQUIRE( npts_lda == npts_gga );

  const int npts = npts_lda;

  XCKernel func( backend, kern, polar ); 

  size_t len_rho_buffer    = func.rho_buffer_len(npts);
  size_t len_sigma_buffer  = func.sigma_buffer_len(npts);
  size_t len_exc_buffer    = func.exc_buffer_len(npts);
  size_t len_vrho_buffer   = func.vrho_buffer_len(npts);
  size_t len_vsigma_buffer = func.vsigma_buffer_len(npts);


  std::vector<double> rho_small(len_rho_buffer, 1e-13);
  std::vector<double> sigma_small(len_sigma_buffer, 1e-14);

  std::vector<double> rho_zero(len_rho_buffer, 0.);
  std::vector<double> sigma_zero(len_sigma_buffer, 0.);

  std::vector<double> rho, sigma;

  if( evaltype == EvalType::Regular ) {
    rho   = ref_rho;
    sigma = ref_sigma;
  }
 
  if( evaltype == EvalType::Small ) {
    rho   = rho_small;
    sigma = sigma_small;
  }
 
  if( evaltype == EvalType::Zero ) {
    rho   = rho_zero;
    sigma = sigma_zero;
  }

  // Get Reference Values
  std::vector<double> 
    exc_ref( len_exc_buffer ),
    vrho_ref( len_vrho_buffer ),
    vsigma_ref( len_vsigma_buffer );

  if( interface == TestInterface::EXC or interface == TestInterface::EXC_INC ) {

    if( func.is_lda() )
      func.eval_exc( npts, rho.data(), exc_ref.data() );
    else if( func.is_gga() )
      func.eval_exc( npts, rho.data(), sigma.data(), exc_ref.data() ); 

  } else if( interface == TestInterface::EXC_VXC or interface == TestInterface::EXC_VXC_INC ) {

    if( func.is_lda() )
      func.eval_exc_vxc( npts, rho.data(), exc_ref.data(), vrho_ref.data() );
    else if( func.is_gga() )
      func.eval_exc_vxc( npts, rho.data(), sigma.data(), exc_ref.data(), 
        vrho_ref.data(), vsigma_ref.data() );
  
  }   






  // Allocate device memory
  double* rho_device    = safe_cuda_malloc<double>( len_rho_buffer    );
  double* sigma_device  = safe_cuda_malloc<double>( len_sigma_buffer  );
  double* exc_device    = safe_cuda_malloc<double>( len_exc_buffer    );
  double* vrho_device   = safe_cuda_malloc<double>( len_vrho_buffer   );
  double* vsigma_device = safe_cuda_malloc<double>( len_vsigma_buffer );

  // H2D Copy of rho / sigma
  safe_cuda_cpy( rho_device, rho.data(), len_rho_buffer );
  if( func.is_gga() )
    safe_cuda_cpy( sigma_device, sigma.data(), len_sigma_buffer );

  const double alpha = 3.14;
  const double fill_val_e = 2.;
  const double fill_val_vr = 10.;
  const double fill_val_vs = 50.;
  
  std::vector<double> 
    exc( len_exc_buffer, fill_val_e ), vrho( len_vrho_buffer, fill_val_vr ),
    vsigma( len_vsigma_buffer, fill_val_vs );

  // H2D copy of initial values, tests clobber / increment
  safe_cuda_cpy( exc_device, exc.data(), len_exc_buffer );
  safe_cuda_cpy( vrho_device, vrho.data(), len_vrho_buffer );
  if( func.is_gga() )
    safe_cuda_cpy( vsigma_device, vsigma.data(), len_vsigma_buffer );

  // Evaluate functional on device
  cudaStream_t stream = 0;
  if( interface == TestInterface::EXC ) {
  
    if( func.is_lda() )
      func.eval_exc_device( npts, rho_device, exc_device, stream );
    else if( func.is_gga() )
      func.eval_exc_device( npts, rho_device, sigma_device, exc_device, 
        stream );

  } else if( interface == TestInterface::EXC_INC ) {

    if( func.is_lda() )
      func.eval_exc_inc_device( alpha, npts, rho_device, exc_device, stream );
    else if( func.is_gga() )
      func.eval_exc_inc_device( alpha, npts, rho_device, sigma_device, exc_device, 
        stream );

  } else if( interface == TestInterface::EXC_VXC ) {

    if( func.is_lda() )
      func.eval_exc_vxc_device( npts, rho_device, exc_device, vrho_device, stream );
    else if( func.is_gga() )
      func.eval_exc_vxc_device( npts, rho_device, sigma_device, exc_device, 
        vrho_device, vsigma_device, stream );

  } else if( interface == TestInterface::EXC_VXC_INC ) {

    if( func.is_lda() )
      func.eval_exc_vxc_inc_device( alpha, npts, rho_device, exc_device, 
        vrho_device, stream );
    else if( func.is_gga() )
      func.eval_exc_vxc_inc_device( alpha, npts, rho_device, sigma_device, 
        exc_device, vrho_device, vsigma_device, stream );

  }

  device_synchronize();

  // D2H of results
  safe_cuda_cpy( exc.data(), exc_device, len_exc_buffer );
  safe_cuda_cpy( vrho.data(), vrho_device, len_vrho_buffer );
  if(func.is_gga()) 
    safe_cuda_cpy( vsigma.data(), vsigma_device, len_vsigma_buffer );

  // Check correctness
  if( interface == TestInterface::EXC_INC or interface == TestInterface::EXC_VXC_INC ) {
    for( auto i = 0ul; i < len_exc_buffer; ++i )
      CHECK( exc[i] == Approx(fill_val_e + alpha * exc_ref[i]) );
  } else {
    for( auto i = 0ul; i < len_exc_buffer; ++i )
      CHECK( exc[i] == Approx(exc_ref[i]) );
  }

  if( interface == TestInterface::EXC_VXC_INC ) {

    for( auto i = 0ul; i < len_vrho_buffer; ++i )
      CHECK( vrho[i] == Approx(fill_val_vr + alpha * vrho_ref[i]) );
    for( auto i = 0ul; i < len_vsigma_buffer; ++i )
      CHECK( vsigma[i] == Approx(fill_val_vs + alpha * vsigma_ref[i]) );

  } else if(interface == TestInterface::EXC_VXC)  {

    for( auto i = 0ul; i < len_vrho_buffer; ++i )
      CHECK( vrho[i] == Approx(vrho_ref[i]) );
    for( auto i = 0ul; i < len_vsigma_buffer; ++i ) {
      INFO( "Kernel is " << kern );
      CHECK( vsigma[i] == Approx(vsigma_ref[i]) );
    }

  }

  cuda_free_all( rho_device, sigma_device, exc_device, vrho_device, vsigma_device );
}



TEST_CASE( "CUDA Interfaces", "[xc-device]" ) {

  SECTION( "Libxc Functionals" ) {

    SECTION( "LDA Functionals: EXC Regular Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Unpolarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Regular Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC Regular Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Regular Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "LDA Functionals: EXC Small Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::libxc, kern, Spin::Unpolarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Small Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC Small Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Small Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "LDA Functionals: EXC Zero Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Unpolarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Zero Eval Unpolarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC Zero Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Unpolarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Zero Eval Unpolarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Unpolarized );
    }








    SECTION( "LDA Functionals: EXC Regular Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Polarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Regular Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC Regular Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Regular Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "LDA Functionals: EXC Small Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::libxc, kern, Spin::Polarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Small Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC Small Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Small Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "LDA Functionals: EXC Zero Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Polarized );
    }


    SECTION( "LDA Functionals: EXC + VXC Zero Eval Polarized" ) {
      for( auto kern : lda_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC Zero Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Polarized );
    }

    SECTION( "GGA Functionals: EXC + VXC Zero Eval Polarized" ) {
      for( auto kern : gga_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::libxc, kern, Spin::Polarized );
    }

  }

  SECTION( "Builtin Functionals" ) {

    SECTION("EXC Regular: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC Regular: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + INC Regular: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Regular,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC + INC Regular: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Regular,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC Small: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC Small: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + INC Small: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Small,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC + INC Small: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Small,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC Zero: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC Zero: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + INC Zero: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Zero,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC + VXC + INC Zero: Unpolarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Zero,
          Backend::builtin, kern, Spin::Unpolarized );
    }

    SECTION("EXC Regular: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Regular,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC Regular: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Regular,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + INC Regular: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Regular,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC + INC Regular: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Regular,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC Small: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Small,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC Small: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Small,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + INC Small: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Small,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC + INC Small: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Small,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC Zero: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC, EvalType::Zero,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC Zero: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC, EvalType::Zero,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + INC Zero: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_INC, EvalType::Zero,
          Backend::builtin, kern, Spin::Polarized );
    }

    SECTION("EXC + VXC + INC Zero: Polarized") {
      for( auto kern : builtin_supported_kernels )
        test_cuda_interface( TestInterface::EXC_VXC_INC, EvalType::Zero,
          Backend::builtin, kern, Spin::Polarized );
    }

  }


}

#endif
