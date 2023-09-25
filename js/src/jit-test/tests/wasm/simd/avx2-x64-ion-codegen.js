// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator") || !isAvxPresent(); include:codegen-x64-test.js

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
// (see codegenTestX64_v128xv128_v128_avxhack comment about AVX defect)
// v128 OP const -> v128
// inputs: [[complete-opname, const, expected-pattern], ...]
function codegenTestX64_v128xLITERAL_v128_avxhack(inputs, options = {}) {
     for ( let [op, const_, expected] of inputs ) {
         codegenTestX64_adhoc(wrap(options, `
         (func (export "f") (param v128 v128) (result v128)
           (${op} (local.get 1) ${const_}))`),
                              'f',
                              expected,
                              options);
     }
}
// (see codegenTestX64_v128xv128_v128_avxhack comment about AVX defect)
// const OP v128 -> v128
// inputs: [[complete-opname, const, expected-pattern], ...]
function codegenTestX64_LITERALxv128_v128_avxhack(inputs, options = {}) {
     for ( let [op, const_, expected] of inputs ) {
         codegenTestX64_adhoc(wrap(options, `
         (func (export "f") (param v128 v128) (result v128)
           (${op} ${const_} (local.get 1)))`),
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

// Machers for any 64- and 32-bit registers.
var GPR_I64 = "%r\\w+";
var GPR_I32 = "%(?:e\\w+|r\\d+d)";

// Simple binary ops: e.g. add, sub, mul
codegenTestX64_v128xv128_v128_avxhack(
     [['i8x16.avgr_u',    `c5 f1 e0 c2               vpavgb %xmm2, %xmm1, %xmm0`],
      ['i16x8.avgr_u',    `c5 f1 e3 c2               vpavgw %xmm2, %xmm1, %xmm0`],
      ['i8x16.add',       `c5 f1 fc c2               vpaddb %xmm2, %xmm1, %xmm0`],
      ['i8x16.add_sat_s', `c5 f1 ec c2               vpaddsb %xmm2, %xmm1, %xmm0`],
      ['i8x16.add_sat_u', `c5 f1 dc c2               vpaddusb %xmm2, %xmm1, %xmm0`],
      ['i8x16.sub',       `c5 f1 f8 c2               vpsubb %xmm2, %xmm1, %xmm0`],
      ['i8x16.sub_sat_s', `c5 f1 e8 c2               vpsubsb %xmm2, %xmm1, %xmm0`],
      ['i8x16.sub_sat_u', `c5 f1 d8 c2               vpsubusb %xmm2, %xmm1, %xmm0`],
      ['i16x8.mul',       `c5 f1 d5 c2               vpmullw %xmm2, %xmm1, %xmm0`],
      ['i16x8.min_s',     `c5 f1 ea c2               vpminsw %xmm2, %xmm1, %xmm0`],
      ['i16x8.min_u',     `c4 e2 71 3a c2            vpminuw %xmm2, %xmm1, %xmm0`],
      ['i16x8.max_s',     `c5 f1 ee c2               vpmaxsw %xmm2, %xmm1, %xmm0`],
      ['i16x8.max_u',     `c4 e2 71 3e c2            vpmaxuw %xmm2, %xmm1, %xmm0`],
      ['i32x4.add',       `c5 f1 fe c2               vpaddd %xmm2, %xmm1, %xmm0`],
      ['i32x4.sub',       `c5 f1 fa c2               vpsubd %xmm2, %xmm1, %xmm0`],
      ['i32x4.mul',       `c4 e2 71 40 c2            vpmulld %xmm2, %xmm1, %xmm0`],
      ['i32x4.min_s',     `c4 e2 71 39 c2            vpminsd %xmm2, %xmm1, %xmm0`],
      ['i32x4.min_u',     `c4 e2 71 3b c2            vpminud %xmm2, %xmm1, %xmm0`],
      ['i32x4.max_s',     `c4 e2 71 3d c2            vpmaxsd %xmm2, %xmm1, %xmm0`],
      ['i32x4.max_u',     `c4 e2 71 3f c2            vpmaxud %xmm2, %xmm1, %xmm0`],
      ['i64x2.add',       `c5 f1 d4 c2               vpaddq %xmm2, %xmm1, %xmm0`],
      ['i64x2.sub',       `c5 f1 fb c2               vpsubq %xmm2, %xmm1, %xmm0`],
      ['i64x2.mul', `
c5 e1 73 d1 20            vpsrlq \\$0x20, %xmm1, %xmm3
66 0f f4 da               pmuludq %xmm2, %xmm3
c5 81 73 d2 20            vpsrlq \\$0x20, %xmm2, %xmm15
66 44 0f f4 f9            pmuludq %xmm1, %xmm15
66 44 0f d4 fb            paddq %xmm3, %xmm15
66 41 0f 73 f7 20         psllq \\$0x20, %xmm15
c5 f1 f4 c2               vpmuludq %xmm2, %xmm1, %xmm0
66 41 0f d4 c7            paddq %xmm15, %xmm0`],
      ['f32x4.add',            `c5 f0 58 c2               vaddps %xmm2, %xmm1, %xmm0`],
      ['f32x4.sub',            `c5 f0 5c c2               vsubps %xmm2, %xmm1, %xmm0`],
      ['f32x4.mul',            `c5 f0 59 c2               vmulps %xmm2, %xmm1, %xmm0`],
      ['f32x4.div',            `c5 f0 5e c2               vdivps %xmm2, %xmm1, %xmm0`],
      ['f64x2.add',            `c5 f1 58 c2               vaddpd %xmm2, %xmm1, %xmm0`],
      ['f64x2.sub',            `c5 f1 5c c2               vsubpd %xmm2, %xmm1, %xmm0`],
      ['f64x2.mul',            `c5 f1 59 c2               vmulpd %xmm2, %xmm1, %xmm0`],
      ['f64x2.div',            `c5 f1 5e c2               vdivpd %xmm2, %xmm1, %xmm0`],
      ['i8x16.narrow_i16x8_s', `c5 f1 63 c2               vpacksswb %xmm2, %xmm1, %xmm0`],
      ['i8x16.narrow_i16x8_u', `c5 f1 67 c2               vpackuswb %xmm2, %xmm1, %xmm0`],
      ['i16x8.narrow_i32x4_s', `c5 f1 6b c2               vpackssdw %xmm2, %xmm1, %xmm0`],
      ['i16x8.narrow_i32x4_u', `c4 e2 71 2b c2            vpackusdw %xmm2, %xmm1, %xmm0`],
      ['i32x4.dot_i16x8_s',    `c5 f1 f5 c2               vpmaddwd %xmm2, %xmm1, %xmm0`]]);

// Simple comparison ops
codegenTestX64_v128xv128_v128_avxhack(
     [['i8x16.eq', `c5 f1 74 c2               vpcmpeqb %xmm2, %xmm1, %xmm0`],
      ['i8x16.ne', `
c5 f1 74 c2               vpcmpeqb %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i8x16.lt_s', `c5 e9 64 c1               vpcmpgtb %xmm1, %xmm2, %xmm0`],
      ['i8x16.gt_u', `
c5 f1 de c2               vpmaxub %xmm2, %xmm1, %xmm0
66 0f 74 c2               pcmpeqb %xmm2, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i16x8.eq', `c5 f1 75 c2               vpcmpeqw %xmm2, %xmm1, %xmm0`],
      ['i16x8.ne', `
c5 f1 75 c2               vpcmpeqw %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i16x8.le_s', `
c5 f1 65 c2               vpcmpgtw %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i16x8.ge_u', `
c4 e2 71 3a c2            vpminuw %xmm2, %xmm1, %xmm0
66 0f 75 c2               pcmpeqw %xmm2, %xmm0`],
      ['i32x4.eq', `c5 f1 76 c2               vpcmpeqd %xmm2, %xmm1, %xmm0`],
      ['i32x4.ne', `
c5 f1 76 c2               vpcmpeqd %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i32x4.lt_s', `c5 e9 66 c1               vpcmpgtd %xmm1, %xmm2, %xmm0`],
      ['i32x4.gt_u', `
c4 e2 71 3f c2            vpmaxud %xmm2, %xmm1, %xmm0
66 0f 76 c2               pcmpeqd %xmm2, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i64x2.eq', `c4 e2 71 29 c2            vpcmpeqq %xmm2, %xmm1, %xmm0`],
      ['i64x2.ne', `
c4 e2 71 29 c2            vpcmpeqq %xmm2, %xmm1, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i64x2.lt_s', `c4 e2 69 37 c1            vpcmpgtq %xmm1, %xmm2, %xmm0`],
      ['i64x2.ge_s', `
c4 e2 69 37 c1            vpcmpgtq %xmm1, %xmm2, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['f32x4.eq', `c5 f0 c2 c2 00            vcmpps \\$0x00, %xmm2, %xmm1, %xmm0`],
      ['f32x4.lt', `c5 f0 c2 c2 01            vcmpps \\$0x01, %xmm2, %xmm1, %xmm0`],
      ['f32x4.ge', `c5 e8 c2 c1 02            vcmpps \\$0x02, %xmm1, %xmm2, %xmm0`],
      ['f64x2.eq', `c5 f1 c2 c2 00            vcmppd \\$0x00, %xmm2, %xmm1, %xmm0`],
      ['f64x2.lt', `c5 f1 c2 c2 01            vcmppd \\$0x01, %xmm2, %xmm1, %xmm0`],
      ['f64x2.ge', `c5 e9 c2 c1 02            vcmppd \\$0x02, %xmm1, %xmm2, %xmm0`],
      ['f32x4.pmin', `c5 e8 5d c1               vminps %xmm1, %xmm2, %xmm0`],
      ['f32x4.pmax', `c5 e8 5f c1               vmaxps %xmm1, %xmm2, %xmm0`],
      ['f64x2.pmin', `c5 e9 5d c1               vminpd %xmm1, %xmm2, %xmm0`],
      ['f64x2.pmax', `c5 e9 5f c1               vmaxpd %xmm1, %xmm2, %xmm0`],
      ['i8x16.swizzle', `
c5 69 dc 3d ${RIPRADDR}   vpaddusbx ${RIPR}, %xmm2, %xmm15
c4 c2 71 00 c7            vpshufb %xmm15, %xmm1, %xmm0`],
      ['i16x8.extmul_high_i8x16_s', `
66 44 0f 3a 0f fa 08      palignr \\$0x08, %xmm2, %xmm15
c4 42 79 20 ff            vpmovsxbw %xmm15, %xmm15
66 0f 3a 0f c1 08         palignr \\$0x08, %xmm1, %xmm0
c4 e2 79 20 c0            vpmovsxbw %xmm0, %xmm0
66 41 0f d5 c7            pmullw %xmm15, %xmm0`],
      ['i32x4.extmul_low_i16x8_u', `
c5 71 e4 fa               vpmulhuw %xmm2, %xmm1, %xmm15
c5 f1 d5 c2               vpmullw %xmm2, %xmm1, %xmm0
66 41 0f 61 c7            punpcklwd %xmm15, %xmm0`],
      ['i64x2.extmul_low_i32x4_s', `
c5 79 70 f9 10            vpshufd \\$0x10, %xmm1, %xmm15
c5 f9 70 c2 10            vpshufd \\$0x10, %xmm2, %xmm0
66 41 0f 38 28 c7         pmuldq %xmm15, %xmm0`],
      ['i16x8.q15mulr_sat_s', `
c4 e2 71 0b c2            vpmulhrsw %xmm2, %xmm1, %xmm0
c5 79 75 3d ${RIPRADDR}   vpcmpeqwx ${RIPR}, %xmm0, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
]);

// Bitwise binary ops
codegenTestX64_v128xv128_v128_avxhack(
     [['v128.and', `c5 f1 db c2               vpand %xmm2, %xmm1, %xmm0`],
      ['v128.andnot', `c5 e9 df c1               vpandn %xmm1, %xmm2, %xmm0`],
      ['v128.or', `c5 f1 eb c2               vpor %xmm2, %xmm1, %xmm0`],
      ['v128.xor', `c5 f1 ef c2               vpxor %xmm2, %xmm1, %xmm0`]]);


// Replace lane ops.
codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i32) (result v128)
          (i8x16.replace_lane 7 (local.get 1) (local.get 2))))`, 'f', `
c4 .. 71 20 .. 07         vpinsrb \\$0x07, ${GPR_I32}, %xmm1, %xmm0`);
codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i32) (result v128)
          (i16x8.replace_lane 3 (local.get 1) (local.get 2))))`, 'f', `
(?:c4 .. 71|c5 f1) c4 .. 03            vpinsrw \\$0x03, ${GPR_I32}, %xmm1, %xmm0`);
codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i32) (result v128)
          (i32x4.replace_lane 2 (local.get 1) (local.get 2))))`, 'f', `
c4 .. 71 22 .. 02         vpinsrd \\$0x02, ${GPR_I32}, %xmm1, %xmm0`);
codegenTestX64_adhoc(`(module
     (func (export "f") (param v128 v128 i64) (result v128)
          (i64x2.replace_lane 1 (local.get 1) (local.get 2))))`, 'f', `
c4 .. f1 22 .. 01         vpinsrq \\$0x01, ${GPR_I64}, %xmm1, %xmm0`);


if (isAvxPresent(2)) {
     codegenTestX64_T_v128_avxhack(
          [['i32', 'i8x16.splat', `
c5 f9 6e ..               vmovd ${GPR_I32}, %xmm0
c4 e2 79 78 c0            vpbroadcastb %xmm0, %xmm0`],
           ['i32', 'i16x8.splat', `
c5 f9 6e ..               vmovd ${GPR_I32}, %xmm0
c4 e2 79 79 c0            vpbroadcastw %xmm0, %xmm0`],
           ['i32', 'i32x4.splat', `
c5 f9 6e ..               vmovd ${GPR_I32}, %xmm0
c4 e2 79 58 c0            vpbroadcastd %xmm0, %xmm0`],
           ['i64', 'i64x2.splat', `
c4 e1 f9 6e ..            vmovq ${GPR_I64}, %xmm0
c4 e2 79 59 c0            vpbroadcastq %xmm0, %xmm0`],
           ['f32', 'f32x4.splat', `c4 e2 79 18 c0            vbroadcastss %xmm0, %xmm0`]], {log:true});

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

// Constant arguments that are folded into the instruction
codegenTestX64_v128xLITERAL_v128_avxhack(
     [['i8x16.add', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 fc 05 ${RIPRADDR}   vpaddbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.sub', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 f8 05 ${RIPRADDR}   vpsubbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.add_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 ec 05 ${RIPRADDR}   vpaddsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.add_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 dc 05 ${RIPRADDR}   vpaddusbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.sub_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 e8 05 ${RIPRADDR}   vpsubsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.sub_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 d8 05 ${RIPRADDR}   vpsubusbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.min_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c4 e2 71 38 05 ${RIPRADDR} vpminsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.min_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 da 05 ${RIPRADDR}   vpminubx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.max_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3c 05 ${RIPRADDR} vpmaxsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.max_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 de 05 ${RIPRADDR}   vpmaxubx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.eq', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 74 05 ${RIPRADDR}   vpcmpeqbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.ne', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
 c5 f1 74 05 ${RIPRADDR}   vpcmpeqbx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i8x16.gt_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 64 05 ${RIPRADDR}   vpcmpgtbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.le_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
 c5 f1 64 05 ${RIPRADDR}   vpcmpgtbx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i8x16.narrow_i16x8_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 63 05 ${RIPRADDR}  vpacksswbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.narrow_i16x8_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 67 05 ${RIPRADDR}  vpackuswbx ${RIPR}, %xmm1, %xmm0`],

      ['i16x8.add', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 fd 05 ${RIPRADDR}  vpaddwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.sub', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 f9 05 ${RIPRADDR}  vpsubwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.mul', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 d5 05 ${RIPRADDR}  vpmullwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.add_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ed 05 ${RIPRADDR}  vpaddswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.add_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 dd 05 ${RIPRADDR}  vpadduswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.sub_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 e9 05 ${RIPRADDR}  vpsubswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.sub_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 d9 05 ${RIPRADDR}  vpsubuswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.min_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ea 05 ${RIPRADDR}  vpminswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.min_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3a 05 ${RIPRADDR} vpminuwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.max_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ee 05 ${RIPRADDR}  vpmaxswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.max_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3e 05 ${RIPRADDR} vpmaxuwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.eq', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 75 05 ${RIPRADDR}  vpcmpeqwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.ne', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
 c5 f1 75 05 ${RIPRADDR}  vpcmpeqwx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i16x8.gt_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 65 05 ${RIPRADDR}  vpcmpgtwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.le_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
 c5 f1 65 05 ${RIPRADDR}  vpcmpgtwx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i16x8.narrow_i32x4_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 6b 05 ${RIPRADDR}  vpackssdwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.narrow_i32x4_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c4 e2 71 2b 05 ${RIPRADDR} vpackusdwx ${RIPR}, %xmm1, %xmm0`],

      ['i32x4.add', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 fe 05 ${RIPRADDR}  vpadddx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.sub', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 fa 05 ${RIPRADDR}  vpsubdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.mul', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 40 05 ${RIPRADDR} vpmulldx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.min_s', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 39 05 ${RIPRADDR} vpminsdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.min_u', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3b 05 ${RIPRADDR} vpminudx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.max_s', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3d 05 ${RIPRADDR} vpmaxsdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.max_u', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3f 05 ${RIPRADDR} vpmaxudx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.eq', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 76 05 ${RIPRADDR}  vpcmpeqdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.ne', '(v128.const i32x4 1 2 1 2)', `
 c5 f1 76 05 ${RIPRADDR}  vpcmpeqdx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i32x4.gt_s', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 66 05 ${RIPRADDR}  vpcmpgtdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.le_s', '(v128.const i32x4 1 2 1 2)', `
 c5 f1 66 05 ${RIPRADDR}  vpcmpgtdx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i32x4.dot_i16x8_s', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 f5 05 ${RIPRADDR}  vpmaddwdx ${RIPR}, %xmm1, %xmm0`],

      ['i64x2.add', '(v128.const i64x2 1 2)',
       `c5 f1 d4 05 ${RIPRADDR}  vpaddqx ${RIPR}, %xmm1, %xmm0`],
      ['i64x2.sub', '(v128.const i64x2 1 2)',
       `c5 f1 fb 05 ${RIPRADDR}  vpsubqx ${RIPR}, %xmm1, %xmm0`],

      ['v128.and', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 db 05 ${RIPRADDR}  vpandx ${RIPR}, %xmm1, %xmm0`],
      ['v128.or', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 eb 05 ${RIPRADDR}  vporx ${RIPR}, %xmm1, %xmm0`],
      ['v128.xor', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 ef 05 ${RIPRADDR}  vpxorx ${RIPR}, %xmm1, %xmm0`],

      ['f32x4.add', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 58 05 ${RIPRADDR}      vaddpsx ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.sub', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 5c 05 ${RIPRADDR}      vsubpsx ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.mul', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 59 05 ${RIPRADDR}      vmulpsx ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.div', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 5e 05 ${RIPRADDR}      vdivpsx ${RIPR}, %xmm1, %xmm0`],

      ['f64x2.add', '(v128.const f64x2 1 2)',
       `c5 f1 58 05 ${RIPRADDR}      vaddpdx ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.sub', '(v128.const f64x2 1 2)',
       `c5 f1 5c 05 ${RIPRADDR}      vsubpdx ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.mul', '(v128.const f64x2 1 2)',
       `c5 f1 59 05 ${RIPRADDR}      vmulpdx ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.div', '(v128.const f64x2 1 2)',
       `c5 f1 5e 05 ${RIPRADDR}      vdivpdx ${RIPR}, %xmm1, %xmm0`],

      ['f32x4.eq', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 c2 05 ${RIPRADDR} 00   vcmppsx \\$0x00, ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.ne', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 c2 05 ${RIPRADDR} 04   vcmppsx \\$0x04, ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.lt', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 c2 05 ${RIPRADDR} 01   vcmppsx \\$0x01, ${RIPR}, %xmm1, %xmm0`],
      ['f32x4.le', '(v128.const f32x4 1 2 3 4)',
       `c5 f0 c2 05 ${RIPRADDR} 02   vcmppsx \\$0x02, ${RIPR}, %xmm1, %xmm0`],

      ['f64x2.eq', '(v128.const f64x2 1 2)',
       `c5 f1 c2 05 ${RIPRADDR} 00  vcmppdx \\$0x00, ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.ne', '(v128.const f64x2 1 2)',
       `c5 f1 c2 05 ${RIPRADDR} 04  vcmppdx \\$0x04, ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.lt', '(v128.const f64x2 1 2)',
       `c5 f1 c2 05 ${RIPRADDR} 01  vcmppdx \\$0x01, ${RIPR}, %xmm1, %xmm0`],
      ['f64x2.le', '(v128.const f64x2 1 2)',
       `c5 f1 c2 05 ${RIPRADDR} 02  vcmppdx \\$0x02, ${RIPR}, %xmm1, %xmm0`]]);

 // Commutative operations with constants on the lhs should generate the same
 // code as with the constant on the rhs.
 codegenTestX64_LITERALxv128_v128_avxhack(
     [['i8x16.add', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 fc 05 ${RIPRADDR}  vpaddbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.add_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 ec 05 ${RIPRADDR}  vpaddsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.add_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 dc 05 ${RIPRADDR}  vpaddusbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.min_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c4 e2 71 38 05 ${RIPRADDR} vpminsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.min_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 da 05 ${RIPRADDR}  vpminubx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.max_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3c 05 ${RIPRADDR} vpmaxsbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.max_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 de 05 ${RIPRADDR}  vpmaxubx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.eq', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 74 05 ${RIPRADDR}  vpcmpeqbx ${RIPR}, %xmm1, %xmm0`],
      ['i8x16.ne', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
 c5 f1 74 05 ${RIPRADDR}  vpcmpeqbx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],

      ['i16x8.add', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 fd 05 ${RIPRADDR}  vpaddwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.mul', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 d5 05 ${RIPRADDR}  vpmullwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.add_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ed 05 ${RIPRADDR}  vpaddswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.add_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 dd 05 ${RIPRADDR}  vpadduswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.min_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ea 05 ${RIPRADDR}  vpminswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.min_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3a 05 ${RIPRADDR} vpminuwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.max_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 ee 05 ${RIPRADDR}  vpmaxswx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.max_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c4 e2 71 3e 05 ${RIPRADDR} vpmaxuwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.eq', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
       `c5 f1 75 05 ${RIPRADDR}  vpcmpeqwx ${RIPR}, %xmm1, %xmm0`],
      ['i16x8.ne', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
 c5 f1 75 05 ${RIPRADDR}  vpcmpeqwx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],

      ['i32x4.add', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 fe 05 ${RIPRADDR}  vpadddx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.mul', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 40 05 ${RIPRADDR} vpmulldx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.min_s', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 39 05 ${RIPRADDR} vpminsdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.min_u', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3b 05 ${RIPRADDR} vpminudx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.max_s', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3d 05 ${RIPRADDR} vpmaxsdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.max_u', '(v128.const i32x4 1 2 1 2)',
       `c4 e2 71 3f 05 ${RIPRADDR} vpmaxudx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.eq', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 76 05 ${RIPRADDR}  vpcmpeqdx ${RIPR}, %xmm1, %xmm0`],
      ['i32x4.ne', '(v128.const i32x4 1 2 1 2)', `
 c5 f1 76 05 ${RIPRADDR}  vpcmpeqdx ${RIPR}, %xmm1, %xmm0
 66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
 66 41 0f ef c7            pxor %xmm15, %xmm0`],
      ['i32x4.dot_i16x8_s', '(v128.const i32x4 1 2 1 2)',
       `c5 f1 f5 05 ${RIPRADDR}  vpmaddwdx ${RIPR}, %xmm1, %xmm0`],

      ['i64x2.add', '(v128.const i64x2 1 2)',
       `c5 f1 d4 05 ${RIPRADDR}  vpaddqx ${RIPR}, %xmm1, %xmm0`],

      ['v128.and', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 db 05 ${RIPRADDR}  vpandx ${RIPR}, %xmm1, %xmm0`],
      ['v128.or', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 eb 05 ${RIPRADDR}  vporx ${RIPR}, %xmm1, %xmm0`],
      ['v128.xor', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
       `c5 f1 ef 05 ${RIPRADDR}  vpxorx ${RIPR}, %xmm1, %xmm0`]]);

// Shift by constant encodings
codegenTestX64_v128xLITERAL_v128_avxhack(
     [['i8x16.shl', '(i32.const 2)', `
c5 f1 fc c1               vpaddb %xmm1, %xmm1, %xmm0
66 0f fc c0               paddb %xmm0, %xmm0`],
      ['i8x16.shl', '(i32.const 4)', `
c5 f1 db 05 ${RIPRADDR}   vpandx ${RIPR}, %xmm1, %xmm0
66 0f 71 f0 04            psllw \\$0x04, %xmm0`],
      ['i16x8.shl', '(i32.const 1)',
       'c5 f9 71 f1 01            vpsllw \\$0x01, %xmm1, %xmm0'],
      ['i16x8.shr_s', '(i32.const 3)',
       'c5 f9 71 e1 03            vpsraw \\$0x03, %xmm1, %xmm0'],
      ['i16x8.shr_u', '(i32.const 2)',
       'c5 f9 71 d1 02            vpsrlw \\$0x02, %xmm1, %xmm0'],
      ['i32x4.shl', '(i32.const 5)',
       'c5 f9 72 f1 05            vpslld \\$0x05, %xmm1, %xmm0'],
      ['i32x4.shr_s', '(i32.const 2)',
       'c5 f9 72 e1 02            vpsrad \\$0x02, %xmm1, %xmm0'],
      ['i32x4.shr_u', '(i32.const 5)',
       'c5 f9 72 d1 05            vpsrld \\$0x05, %xmm1, %xmm0'],
      ['i64x2.shr_s', '(i32.const 7)', `
c5 79 70 f9 f5            vpshufd \\$0xF5, %xmm1, %xmm15
66 41 0f 72 e7 1f         psrad \\$0x1F, %xmm15
c4 c1 71 ef c7            vpxor %xmm15, %xmm1, %xmm0
66 0f 73 d0 07            psrlq \\$0x07, %xmm0
66 41 0f ef c7            pxor %xmm15, %xmm0`]]);

// vpblendvp optimization when bitselect follows comparison.
codegenTestX64_adhoc(
     `(module
         (func (export "f") (param v128) (param v128) (param v128) (param v128) (result v128)
           (v128.bitselect (local.get 2) (local.get 3)
              (i32x4.eq (local.get 0) (local.get 1)))))`,
         'f', `
66 0f 76 c1               pcmpeqd %xmm1, %xmm0
c4 e3 61 4c c2 00         vpblendvb %xmm0, %xmm2, %xmm3, %xmm0`);
