// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator; include:codegen-x64-test.js

// Test that there are no extraneous moves for various SIMD conversion
// operations. See README-codegen.md for general information about this type of
// test case.

// Note, these tests test the beginning of the output but not the end.

codegenTestX64_v128_v128(
    [['i32x4.trunc_sat_f32x4_s',
     // The movaps is dest -> scratch and needs to be here.  The test is
     // asserting that there is not an additional (redundant) move here.
`
44 0f 28 f8               movaps %xmm0, %xmm15
45 0f c2 ff 00            cmpps \\$0x00, %xmm15, %xmm15
66 41 0f db c7            pand %xmm15, %xmm0`],
     ['i32x4.trunc_sat_f32x4_u', `
66 45 0f ef ff            pxor %xmm15, %xmm15
41 0f 5f c7               maxps %xmm15, %xmm0`],
     ['f32x4.convert_i32x4_u', `
66 45 0f ef ff            pxor %xmm15, %xmm15
66 44 0f 3a 0e f8 55      pblendw \\$0x55, %xmm0, %xmm15
66 41 0f fa c7            psubd %xmm15, %xmm0
45 0f 5b ff               cvtdq2ps %xmm15, %xmm15`]],
    {no_suffix:true});


