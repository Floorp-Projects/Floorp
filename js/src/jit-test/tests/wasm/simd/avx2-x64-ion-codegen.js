// |jit-test| --enable-avx; skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator || !isAvxPresent(); include:codegen-x64-test.js

// Test that there are no extraneous moves for various SIMD conversion
// operations. See README-codegen.md for general information about this type of
// test case.

// Note, these tests test the beginning of the output but not the end.

// Currently AVX2 exhibits a defect when function uses its first v128 arg and
// returns v128: the register allocator adds unneeded extra moves from xmm0,
// then into different temporary, and then the latter temporary is used as arg.
// In the tests below, to simplify things, don't use/ignore the first arg.
// v128 OP v128 -> v128
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_v128xv128_v128_avxhack(inputs, options = {}) {
     for ( let [op, expected] of inputs ) {
         codegenTestX64_adhoc(wrap(options, `
         (func (export "f") (param v128 v128 v128) (result v128)
           (${op} (local.get 1) (local.get 2)))`),
                              'f',
                              expected,
                              options);
     }
}

// Utility function to test SIMD operations encoding, where the input argument
// has the specified type (T).
// inputs: [[type, complete-opname, expected-pattern], ...]
function codegenTestX64_T_v128_avxhack(inputs, options = {}) {
     for ( let [ty, op, expected] of inputs ) {
         codegenTestX64_adhoc(wrap(options, `
         (func (export "f") (param ${ty}) (result v128)
           (${op} (local.get 0)))`),
                              'f',
                              expected,
                              options);
     }
}

// Simple binary ops: e.g. add, sub, mul
codegenTestX64_v128xv128_v128_avxhack(
     [['i32x4.add', `c5 f1 fe c2               vpaddd %xmm2, %xmm1, %xmm0`],
      ['i32x4.sub', `c5 f1 fa c2               vpsubd %xmm2, %xmm1, %xmm0`],
      ['i32x4.mul', `c4 e2 71 40 c2            vpmulld %xmm2, %xmm1, %xmm0`],
      ['f32x4.add', `c5 f0 58 c2               vaddps %xmm2, %xmm1, %xmm0`],
      ['f32x4.sub', `c5 f0 5c c2               vsubps %xmm2, %xmm1, %xmm0`],
      ['f32x4.mul', `c5 f0 59 c2               vmulps %xmm2, %xmm1, %xmm0`],
      ['f32x4.div', `c5 f0 5e c2               vdivps %xmm2, %xmm1, %xmm0`]]);

// Simple comparison ops
codegenTestX64_v128xv128_v128_avxhack(
     [['i32x4.eq', `c5 f1 76 c2               vpcmpeqd %xmm2, %xmm1, %xmm0`],
      ['i32x4.ne', `
c5 f1 76 c2               vpcmpeqd %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i32x4.lt_s', `
c5 f9 6f c2               vmovdqa %xmm2, %xmm0
66 0f 66 c1               pcmpgtd %xmm1, %xmm0`],
      ['i32x4.gt_u', `
c4 e2 71 3f c2            vpmaxud %xmm2, %xmm1, %xmm0
66 0f 76 c2               pcmpeqd %xmm2, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['f32x4.eq', `c5 f0 c2 c2 00            vcmpps \\$0x00, %xmm2, %xmm1, %xmm0`],
      ['f32x4.lt', `c5 f0 c2 c2 01            vcmpps \\$0x01, %xmm2, %xmm1, %xmm0`],
      ['f32x4.ge', `
c5 f9 6f c2               vmovdqa %xmm2, %xmm0
0f c2 c1 02               cmpps \\$0x02, %xmm1, %xmm0`]]);

// Bitwise binary ops
codegenTestX64_v128xv128_v128_avxhack(
     [['v128.and', `c5 f1 db c2               vpand %xmm2, %xmm1, %xmm0`],
      ['v128.andnot', `c5 e9 df c1               vpandn %xmm1, %xmm2, %xmm0`],
      ['v128.or', `c5 f1 eb c2               vpor %xmm2, %xmm1, %xmm0`],
      ['v128.xor', `c5 f1 ef c2               vpxor %xmm2, %xmm1, %xmm0`]]);


codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i64) (result v128)
          (i64x2.replace_lane 1 (local.get 1) (local.get 2))))`,
                              'f',
                              `
c4 .. f1 22 .. 01         vpinsrq \\$0x01, %r\\w+, %xmm1, %xmm0` ); // rdi (Linux) or r8 (Win)
     
                             
if (isAvxPresent(2)) {
     // First i32 arg is: edi on Linux, and ecx on Windows.
     codegenTestX64_T_v128_avxhack(
          [['i32', 'i8x16.splat', `
c5 f9 6e ..               vmovd %e\\w+, %xmm0
c4 e2 79 78 c0            vpbroadcastb %xmm0, %xmm0`],
           ['i32', 'i16x8.splat', `
c5 f9 6e ..               vmovd %e\\w+, %xmm0
c4 e2 79 79 c0            vpbroadcastw %xmm0, %xmm0`],
           ['i32', 'i32x4.splat', `
c5 f9 6e ..               vmovd %e\\w+, %xmm0
c4 e2 79 58 c0            vpbroadcastd %xmm0, %xmm0`],
           ['f32', 'f32x4.splat', `c4 e2 79 18 c0            vbroadcastss %xmm0, %xmm0`]]);

     codegenTestX64_T_v128_avxhack(
          [['i32', 'v128.load8_splat',
            'c4 c2 79 78 04 ..         vpbroadcastbb \\(%r15,%r\\w+,1\\), %xmm0'],
           ['i32', 'v128.load16_splat',
            'c4 c2 79 79 04 ..         vpbroadcastww \\(%r15,%r\\w+,1\\), %xmm0'],
           ['i32', 'v128.load32_splat',
            'c4 c2 79 18 04 ..         vbroadcastssl \\(%r15,%r\\w+,1\\), %xmm0']], {memory: 1});
}

// Using VEX during shuffle ops
codegenTestX64_v128xv128_v128_avxhack([
     // Identity op on second argument should generate a move
    ['i8x16.shuffle 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15',
     'c5 f9 6f c1               vmovdqa %xmm1, %xmm0'],

     // Broadcast a byte from first argument
    ['i8x16.shuffle 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5 5',
     `
c5 f1 60 c1               vpunpcklbw %xmm1, %xmm1, %xmm0
c5 fa 70 c0 55            vpshufhw \\$0x55, %xmm0, %xmm0
c5 f9 70 c0 aa            vpshufd \\$0xAA, %xmm0, %xmm0`],

     // Broadcast a word from first argument
    ['i8x16.shuffle 4 5 4 5 4 5 4 5 4 5 4 5 4 5 4 5',
     `
c5 fb 70 c1 aa            vpshuflw \\$0xAA, %xmm1, %xmm0
c5 f9 70 c0 00            vpshufd \\$0x00, %xmm0, %xmm0`],

     // Permute words
     ['i8x16.shuffle 2 3 0 1 6 7 4 5 10 11 8 9 14 15 12 13',
`
c5 fb 70 c1 b1            vpshuflw \\$0xB1, %xmm1, %xmm0
c5 fa 70 c0 b1            vpshufhw \\$0xB1, %xmm0, %xmm0`],

     // Permute doublewords
     ['i8x16.shuffle 4 5 6 7 0 1 2 3 12 13 14 15 8 9 10 11',
      'c5 f9 70 c1 b1            vpshufd \\$0xB1, %xmm1, %xmm0'],

     // Interleave doublewords
     ['i8x16.shuffle 0 1 2 3 16 17 18 19 4 5 6 7 20 21 22 23',
      'c5 f1 62 c2               vpunpckldq %xmm2, %xmm1, %xmm0'],

     // Interleave quadwords
     ['i8x16.shuffle 24 25 26 27 28 29 30 31 8 9 10 11 12 13 14 15',
      'c5 e9 6d c1               vpunpckhqdq %xmm1, %xmm2, %xmm0'],
     
     // Rotate right
    ['i8x16.shuffle 13 14 15 0 1 2 3 4 5 6 7 8 9 10 11 12',
     `c4 e3 71 0f c1 0d         vpalignr \\$0x0D, %xmm1, %xmm1, %xmm0`],
    ['i8x16.shuffle 28 29 30 31 0 1 2 3 4 5 6 7 8 9 10 11',
     `c4 e3 71 0f c2 0c         vpalignr \\$0x0C, %xmm2, %xmm1, %xmm0`]]);

if (isAvxPresent(2)) {
     codegenTestX64_v128xv128_v128_avxhack([
          // Broadcast low byte from second argument
          ['i8x16.shuffle 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0',
           'c4 e2 79 78 c1            vpbroadcastb %xmm1, %xmm0'],

          // Broadcast low word from third argument
          ['i8x16.shuffle 16 17 16 17 16 17 16 17 16 17 16 17 16 17 16 17',
          'c4 e2 79 79 c2            vpbroadcastw %xmm2, %xmm0'],

          // Broadcast low doubleword from second argument
          ['i8x16.shuffle 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3',
           'c4 e2 79 58 c1            vpbroadcastd %xmm1, %xmm0']]);
}

// Testing AVX optimization where VPBLENDVB accepts four XMM registers as args.
codegenTestX64_adhoc(
     `(func (export "f") (param v128 v128 v128 v128) (result v128)
        (i8x16.shuffle 0 17 2 3 4 5 6 7 24 25 26 11 12 13 30 15
          (local.get 2)(local.get 3)))`,
     'f',
`
66 0f 6f 0d ${RIPRADDR}   movdqax ${RIPR}, %xmm1
c4 e3 69 4c c3 10         vpblendvb %xmm1, %xmm3, %xmm2, %xmm0`);
