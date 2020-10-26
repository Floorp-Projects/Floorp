// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves or fixups for SIMD shuffle
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15',
     // Identity op on first argument should generate no code
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31',
     // Identity op on second argument should generate a move
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 6f c1               movdqa %xmm1, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5',
     // Broadcast a byte from first argument
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 60 c0               punpcklbw %xmm0, %xmm0
000000..  f3 0f 70 c0 55            pshufhw \\$0x55, %xmm0, %xmm0
000000..  66 0f 70 c0 aa            pshufd \\$0xAA, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 4 5 4 5 4 5 4 5 4 5 4 5 4 5 4 5',
     // Broadcast a word from first argument
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f2 0f 70 c0 aa            pshuflw \\$0xAA, %xmm0, %xmm0
000000..  66 0f 70 c0 00            pshufd \\$0x00, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 2 1 4 3 6 5 8 7 10 9 12 11 14 13 0 15',
     // Permute bytes
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 44 0f 6f 3d .. 00 00 00 
                                    movdqax 0x0000000000000040, %xmm15
000000..  66 41 0f 38 00 c7         pshufb %xmm15, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 2 3 0 1 6 7 4 5 10 11 8 9 14 15 12 13',
     // Permute words
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f2 0f 70 c0 b1            pshuflw \\$0xB1, %xmm0, %xmm0
000000..  f3 0f 70 c0 b1            pshufhw \\$0xB1, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11',
     // Permute doublewords
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 70 c0 b1            pshufd \\$0xB1, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 13 14 15 0 1 2 3 4 5 6 7 8 9 10 11 12',
     // Rotate right
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 3a 0f c0 0d         palignr \\$0x0D, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 15 29 0 1 2 1 2 0 3 4 7 8 16 8 17 9',
     // General shuffle + blend.  The initial movdqa to scratch is
     // unavoidable unless we can convince the compiler that it's OK to destroy xmm1.
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 44 0f 6f f9            movdqa %xmm1, %xmm15
000000..  66 44 0f 38 00 3d .. 00 00 00 
                                    pshufbx 0x0000000000000050, %xmm15
000000..  66 0f 38 00 05 .. 00 00 00 
                                    pshufbx 0x0000000000000060, %xmm0
000000..  66 41 0f eb c7            por %xmm15, %xmm0
000000..  5d                        pop %rbp
`],
] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 0) (local.get 1))))
        `);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

// Inputs (xmm0, zero)

for ( let [op, expected] of [
    ['i8x16.shuffle 16 16 16 0 1 2 3 4 5 6 7 8 9 10 11 12',
     // Shift left bytes, shifting in zeroes
     //
     // Remember the low-order bytes are at the "right" end
     //
     // The pxor is a code generation bug: the operand is unused, and no
     // code should need to be generated for it, and no register should
     // be allocated to it.  The lowering does not use that operand, but
     // code generation still touches it.
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c9               pxor %xmm1, %xmm1
000000..  66 0f 73 f8 03            pslldq \\$0x03, %xmm0
000000..  5d                        pop %rbp
`],
    ['i8x16.shuffle 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18',
     // Shift right bytes, shifting in zeroes.  See above.
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c9               pxor %xmm1, %xmm1
000000..  66 0f 73 d8 03            psrldq \\$0x03, %xmm0
000000..  5d                        pop %rbp
`],

] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0) (v128.const i32x4 0 0 0 0))))
        `);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}
