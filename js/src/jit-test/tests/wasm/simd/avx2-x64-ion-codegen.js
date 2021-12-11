// |jit-test| --enable-avx; skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator || !isAvxPresent(); include:codegen-x64-test.js

// Test that there are no extraneous moves for various SIMD conversion
// operations. See README-codegen.md for general information about this type of
// test case.

// Note, these tests test the beginning of the output but not the end.


codegenTestX64_adhoc(`(module
 (func (export "f") (param v128 v128 v128) (result v128)
   (i32x4.add (local.get 1) (local.get 2))))`,
                      'f',
                      `
c5 f1 fe c2               vpaddd %xmm2, %xmm1, %xmm0`,
                         {no_suffix:true});

codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 v128) (result v128)
          (i32x4.sub (local.get 1) (local.get 2))))`,
                              'f',
                              `
c5 f1 fa c2               vpsubd %xmm2, %xmm1, %xmm0`,
                              {no_suffix:true});

codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 v128) (result v128)
          (i32x4.mul (local.get 1) (local.get 2))))`,
                              'f',
                              `
c4 e2 71 40 c2           vpmulld %xmm2, %xmm1, %xmm0`,
                              {no_suffix:true});
                              
codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i64) (result v128)
          (i64x2.replace_lane 1 (local.get 1) (local.get 2))))`,
                              'f',
                              `
c4 .. f1 22 .. 01         vpinsrq \\$0x01, %r\\w+, %xmm1, %xmm0`, // rdi (Linux) or r8 (Win)
                              {no_suffix:true});
     
                             
