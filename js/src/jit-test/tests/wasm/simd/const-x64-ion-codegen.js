// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator"); include:codegen-x64-test.js

// Test that constants that can be synthesized are synthesized.  See README-codegen.md
// for general information about this type of test case.

codegenTestX64_unit_v128(
    [['v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0',
      `66 0f ef c0               pxor %xmm0, %xmm0`],
     ['v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1',
      `66 0f 75 c0               pcmpeqw %xmm0, %xmm0`],
     ['v128.const i16x8 0 0 0 0 0 0 0 0',
      `66 0f ef c0               pxor %xmm0, %xmm0`],
     ['v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1',
      `66 0f 75 c0               pcmpeqw %xmm0, %xmm0`],
     ['v128.const i32x4 0 0 0 0',
      `66 0f ef c0               pxor %xmm0, %xmm0`],
     ['v128.const i32x4 -1 -1 -1 -1',
      `66 0f 75 c0               pcmpeqw %xmm0, %xmm0`],
     ['v128.const i64x2 0 0',
      `66 0f ef c0               pxor %xmm0, %xmm0`],
     ['v128.const i64x2 -1 -1',
      `66 0f 75 c0               pcmpeqw %xmm0, %xmm0`],
     ['v128.const f32x4 0 0 0 0',
      // Arguably this should be xorps but that's for later
      `66 0f ef c0               pxor %xmm0, %xmm0`],
     ['v128.const f64x2 0 0',
      // Arguably this should be xorpd but that's for later
      `66 0f ef c0               pxor %xmm0, %xmm0`]] );
