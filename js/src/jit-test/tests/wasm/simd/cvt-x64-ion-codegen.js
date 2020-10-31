// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves for various SIMD conversion
// operations. See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['i32x4.trunc_sat_f32x4_s',
     // The movaps is dest -> scratch and needs to be here.  The test is
     // asserting that there is not an additional (redundant) move here.
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  44 0f 28 f8               movaps %xmm0, %xmm15
000000..  45 0f c2 ff 00            cmpps \\$0x00, %xmm15, %xmm15
000000..  66 41 0f db c7            pand %xmm15, %xmm0
`],
    ['i32x4.trunc_sat_f32x4_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 45 0f ef ff            pxor %xmm15, %xmm15
000000..  41 0f 5f c7               maxps %xmm15, %xmm0
`],
    ['f32x4.convert_i32x4_u', `
00000023  48 8b ec                  mov %rsp, %rbp
00000026  66 45 0f ef ff            pxor %xmm15, %xmm15
0000002B  66 44 0f 3a 0e f8 55      pblendw \\$0x55, %xmm0, %xmm15
00000032  66 41 0f fa c7            psubd %xmm15, %xmm0
00000037  45 0f 5b ff               cvtdq2ps %xmm15, %xmm15
`],
] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0))))`);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}


