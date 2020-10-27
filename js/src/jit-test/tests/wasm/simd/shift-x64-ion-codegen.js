// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves for a constant integer SIMD shift
// that can reuse its input for its output.  See README-codegen.md for general
// information about this type of test case.
//
// There are test cases here for all codegen cases that include a potential move
// to set up the operation, but not for all shift operations in general.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['i8x16.shl', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f fc c0               paddb %xmm0, %xmm0
000000..  66 0f fc c0               paddb %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.shl', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 71 f0 02            psllw \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.shl', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 72 f0 02            pslld \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i64x2.shl', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 73 f0 02            psllq \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shr_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f db 05 .. 00 00 00   pandx 0x00000000000000.., %xmm0
000000..  66 0f 71 d0 02            psrlw \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.shr_s', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 71 e0 02            psraw \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i16x8.shr_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 71 d0 02            psrlw \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.shr_s', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 72 e0 02            psrad \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i32x4.shr_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 72 d0 02            psrld \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
    ['i64x2.shr_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 73 d0 02            psrlq \\$0x02, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0) (i32.const 2))))`);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}


