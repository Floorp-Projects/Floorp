// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator"); include:codegen-x64-test.js

// Test that there are no extraneous moves or fixups for various SIMD comparison
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

codegenTestX64_v128xv128_v128(
    [['i8x16.gt_s', `66 0f 64 c1               pcmpgtb %xmm1, %xmm0`],
     ['i16x8.gt_s', `66 0f 65 c1               pcmpgtw %xmm1, %xmm0`],
     ['i32x4.gt_s', `66 0f 66 c1               pcmpgtd %xmm1, %xmm0`],
     ['i8x16.le_s', `
66 0f 64 c1               pcmpgtb %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['i16x8.le_s', `
66 0f 65 c1               pcmpgtw %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['i32x4.le_s', `
66 0f 66 c1               pcmpgtd %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['i8x16.eq', `66 0f 74 c1               pcmpeqb %xmm1, %xmm0`],
     ['i16x8.eq', `66 0f 75 c1               pcmpeqw %xmm1, %xmm0`],
     ['i32x4.eq', `66 0f 76 c1               pcmpeqd %xmm1, %xmm0`],
     ['i8x16.ne', `
66 0f 74 c1               pcmpeqb %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['i16x8.ne', `
66 0f 75 c1               pcmpeqw %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['i32x4.ne', `
66 0f 76 c1               pcmpeqd %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0
`],
     ['f32x4.eq', `0f c2 c1 00               cmpps \\$0x00, %xmm1, %xmm0`],
     ['f32x4.ne', `0f c2 c1 04               cmpps \\$0x04, %xmm1, %xmm0`],
     ['f32x4.lt', `0f c2 c1 01               cmpps \\$0x01, %xmm1, %xmm0`],
     ['f32x4.le', `0f c2 c1 02               cmpps \\$0x02, %xmm1, %xmm0`],
     ['f64x2.eq', `66 0f c2 c1 00            cmppd \\$0x00, %xmm1, %xmm0`],
     ['f64x2.ne', `66 0f c2 c1 04            cmppd \\$0x04, %xmm1, %xmm0`],
     ['f64x2.lt', `66 0f c2 c1 01            cmppd \\$0x01, %xmm1, %xmm0`],
     ['f64x2.le', `66 0f c2 c1 02            cmppd \\$0x02, %xmm1, %xmm0`]] );

// Inputs (xmm1, xmm0) because the operation reverses its arguments.

codegenTestX64_v128xv128_v128_reversed(
    [['i8x16.ge_s', `
66 0f 64 c1               pcmpgtb %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i16x8.ge_s',
`
66 0f 65 c1               pcmpgtw %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i32x4.ge_s', `
66 0f 66 c1               pcmpgtd %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i8x16.lt_s', `66 0f 64 c1               pcmpgtb %xmm1, %xmm0`],
     ['i16x8.lt_s', `66 0f 65 c1               pcmpgtw %xmm1, %xmm0`],
     ['i32x4.lt_s', `66 0f 66 c1               pcmpgtd %xmm1, %xmm0`],
     ['f32x4.gt', `0f c2 c1 01               cmpps \\$0x01, %xmm1, %xmm0`],
     ['f32x4.ge', `0f c2 c1 02               cmpps \\$0x02, %xmm1, %xmm0`],
     ['f64x2.gt', `66 0f c2 c1 01            cmppd \\$0x01, %xmm1, %xmm0`],
     ['f64x2.ge', `66 0f c2 c1 02            cmppd \\$0x02, %xmm1, %xmm0`]] );
