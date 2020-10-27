// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves for variable SIMD negate
// instructions. See README-codegen.md for general information about this type
// of test case.

// Integer negates don't have to reuse the input for the output, and prefer for
// the registers to be different.

// Inputs (xmm1, xmm0)

for ( let [ op, expected ] of [
    ['i8x16.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  66 0f f8 c1               psubb %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  66 0f f9 c1               psubw %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  66 0f fa c1               psubd %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i64x2.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  66 0f fb c1               psubq %xmm1, %xmm0
000000..  5d                        pop %rbp
`],

] ) {

    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 1))))
`);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

// Floating point negate and absolute value, and bitwise not, prefer for the
// registers to be the same and guarantee that no move is inserted if so.

// Inputs (xmm0, xmm1)

for ( let [ op, expected ] of [
    ['f32x4.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x00000000000000.., %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.neg', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x00000000000000.., %xmm0
000000..  5d                        pop %rbp
`],
    ['f32x4.abs', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f db 05 .. 00 00 00   pandx 0x00000000000000.., %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2.abs', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f db 05 .. 00 00 00   pandx 0x00000000000000.., %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.not', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef 05 .. 00 00 00   pxorx 0x00000000000000.., %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    var ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0))))
`);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}
