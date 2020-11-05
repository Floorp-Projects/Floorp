// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64; include:codegen-x64-test.js

// Test that there are no extraneous moves or fixups for sundry SIMD binary
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

codegenTestX64_v128xPTYPE_v128(
    [['f32x4.replace_lane 0', 'f32', `f3 0f 10 c1               movss %xmm1, %xmm0`],
     ['f32x4.replace_lane 1', 'f32', `66 0f 3a 21 c1 10         insertps \\$0x10, %xmm1, %xmm0`],
     ['f32x4.replace_lane 3', 'f32', `66 0f 3a 21 c1 30         insertps \\$0x30, %xmm1, %xmm0`],
     ['f64x2.replace_lane 0', 'f64', `f2 0f 10 c1               movsd %xmm1, %xmm0`],
     ['f64x2.replace_lane 1', 'f64', `66 0f c6 c1 00            shufpd \\$0x00, %xmm1, %xmm0`]] );

// Inputs (xmm1, xmm0)

codegenTestX64_v128xv128_v128_reversed(
    [['f32x4.pmin', `0f 5d c1                  minps %xmm1, %xmm0`],
     ['f32x4.pmax', `0f 5f c1                  maxps %xmm1, %xmm0`],
     ['f64x2.pmin', `66 0f 5d c1               minpd %xmm1, %xmm0`],
     ['f64x2.pmax', `66 0f 5f c1               maxpd %xmm1, %xmm0`]] );
