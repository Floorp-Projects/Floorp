// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator") || isAvxPresent(); include:codegen-x64-test.js

// Test that there are no extraneous moves or fixups for SIMD shuffle
// operations.  See README-codegen.md for general information about this type of
// test case.

codegenTestX64_v128xv128_v128([
     // Identity op on first argument should generate no code
    ['i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15',
     ''],

     // Identity op on second argument should generate a move
    ['i8x16.shuffle 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31',
     `66 0f 6f c1               movdqa %xmm1, %xmm0`],

     // Broadcast a byte from first argument
    ['i8x16.shuffle 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5',
     `
66 0f 60 c0               punpcklbw %xmm0, %xmm0
f3 0f 70 c0 55            pshufhw \\$0x55, %xmm0, %xmm0
66 0f 70 c0 aa            pshufd \\$0xAA, %xmm0, %xmm0`],

     // Broadcast a word from first argument
    ['i8x16.shuffle 4 5 4 5 4 5 4 5 4 5 4 5 4 5 4 5',
     `
f2 0f 70 c0 aa            pshuflw \\$0xAA, %xmm0, %xmm0
66 0f 70 c0 00            pshufd \\$0x00, %xmm0, %xmm0`],

     // Permute bytes
    ['i8x16.shuffle 2 1 4 3 6 5 8 7 10 9 12 11 14 13 0 15',
`
66 0f 38 00 05 ${RIPRADDR} pshufbx ${RIPR}, %xmm0`],

     // Permute words
    ['i8x16.shuffle 2 3 0 1 6 7 4 5 10 11 8 9 14 15 12 13',
`
f2 0f 70 c0 b1            pshuflw \\$0xB1, %xmm0, %xmm0
f3 0f 70 c0 b1            pshufhw \\$0xB1, %xmm0, %xmm0`],

     // Permute doublewords
    ['i8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11',
     `66 0f 70 c0 b1            pshufd \\$0xB1, %xmm0, %xmm0`],

     // Rotate right
    ['i8x16.shuffle 13 14 15 0 1 2 3 4 5 6 7 8 9 10 11 12',
     `66 0f 3a 0f c0 0d         palignr \\$0x0D, %xmm0, %xmm0`],

     // General shuffle + blend.  The initial movdqa to scratch is unavoidable
     // unless we can convince the compiler that it's OK to destroy xmm1.
    ['i8x16.shuffle 15 29 0 1 2 1 2 0 3 4 7 8 16 8 17 9',
`
66 44 0f 6f f9                movdqa %xmm1, %xmm15
66 44 0f 38 00 3d ${RIPRADDR} pshufbx ${RIPR}, %xmm15
66 0f 38 00 05 ${RIPRADDR}    pshufbx ${RIPR}, %xmm0
66 41 0f eb c7                por %xmm15, %xmm0`]]);

codegenTestX64_v128xLITERAL_v128(
    [// Shift left bytes, shifting in zeroes
     //
     // Remember the low-order bytes are at the "right" end
     //
     // The pxor is a code generation bug: the operand is unused, and no
     // code should need to be generated for it, and no register should
     // be allocated to it.  The lowering does not use that operand, but
     // code generation still touches it.
     ['i8x16.shuffle 16 16 16 0 1 2 3 4 5 6 7 8 9 10 11 12',
      '(v128.const i32x4 0 0 0 0)',
`
66 0f 73 f8 03            pslldq \\$0x03, %xmm0`],

     // Shift right bytes, shifting in zeroes.  See above.
     ['i8x16.shuffle 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18',
      '(v128.const i32x4 0 0 0 0)',
`
66 0f 73 d8 03            psrldq \\$0x03, %xmm0`]]);

// SSE4.1 PBLENDVB instruction is using XMM0, checking if blend
// operation generated as expected.
codegenTestX64_adhoc(
     `(func (export "f") (param v128 v128 v128 v128) (result v128)
        (i8x16.shuffle 0 17 2 3 4 5 6 7 24 25 26 11 12 13 30 15
          (local.get 2)(local.get 3)))`,
     'f',
`
66 0f 6f ca               movdqa %xmm2, %xmm1
66 0f 6f 05 ${RIPRADDR}   movdqax ${RIPR}, %xmm0
66 0f 38 10 cb            pblendvb %xmm3, %xmm1
66 0f 6f c1               movdqa %xmm1, %xmm0`);
