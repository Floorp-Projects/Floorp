// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves or fixups for sundry SIMD binary
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['f32x4.replace_lane 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f3 0f 10 c1               movss %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.replace_lane 1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 3a 21 c1 10         insertps \\$0x10, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.replace_lane 3',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 3a 21 c1 30         insertps \\$0x30, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.replace_lane 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f2 0f 10 c1               movsd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.replace_lane 1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c6 c1 00            shufpd \\$0x00, %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ptype = op.substring(0,3);
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (param ${ptype}) (result v128)
      (${op} (local.get 0) (local.get 1))))
        `);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

// Inputs (xmm1, xmm0)

for ( let [op, expected] of [
    ['f32x4.pmin',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f 5d c1                  minps %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.pmax',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f 5f c1                  maxps %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.pmin',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 5d c1               minpd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.pmax',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 5f c1               maxpd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 1) (local.get 0))))
        `);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}
