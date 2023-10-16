// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator; include:codegen-x64-test.js

// Test that there are no extraneous moves or fixups for sundry SIMD binary
// operations.  See README-codegen.md for general information about this type of
// test case.

// Inputs (xmm0, xmm1)

codegenTestX64_v128xPTYPE_v128(
    [['f32x4.replace_lane 0', 'f32', `f3 0f 10 c1               movss %xmm1, %xmm0`],
     ['f32x4.replace_lane 1', 'f32', `66 0f 3a 21 c1 10         insertps \\$0x10, %xmm1, %xmm0`],
     ['f32x4.replace_lane 3', 'f32', `66 0f 3a 21 c1 30         insertps \\$0x30, %xmm1, %xmm0`],
     ['f64x2.replace_lane 0', 'f64', `f2 0f 10 c1               movsd %xmm1, %xmm0`],
     ['f64x2.replace_lane 1', 'f64', `66 0f c6 c1 00            shufpd \\$0x00, %xmm1, %xmm0`]] );

// Inputs (xmm1, xmm0)

codegenTestX64_v128xv128_v128_reversed(
    [['f32x4.pmin', `0f 5d c1                  minps %xmm1, %xmm0`],
     ['f32x4.pmax', `0f 5f c1                  maxps %xmm1, %xmm0`],
     ['f64x2.pmin', `66 0f 5d c1               minpd %xmm1, %xmm0`],
     ['f64x2.pmax', `66 0f 5f c1               maxpd %xmm1, %xmm0`]] );

// Constant arguments that are folded into the instruction

codegenTestX64_v128xLITERAL_v128(
    [['i8x16.add', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f fc 05 ${RIPRADDR}   paddbx ${RIPR}, %xmm0`],
     ['i8x16.sub', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f f8 05 ${RIPRADDR}   psubbx ${RIPR}, %xmm0`],
     ['i8x16.add_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f ec 05 ${RIPRADDR}   paddsbx ${RIPR}, %xmm0`],
     ['i8x16.add_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f dc 05 ${RIPRADDR}   paddusbx ${RIPR}, %xmm0`],
     ['i8x16.sub_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f e8 05 ${RIPRADDR}   psubsbx ${RIPR}, %xmm0`],
     ['i8x16.sub_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f d8 05 ${RIPRADDR}   psubusbx ${RIPR}, %xmm0`],
     ['i8x16.min_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 38 38 05 ${RIPRADDR} pminsbx ${RIPR}, %xmm0`],
     ['i8x16.min_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f da 05 ${RIPRADDR}   pminubx ${RIPR}, %xmm0`],
     ['i8x16.max_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 38 3c 05 ${RIPRADDR} pmaxsbx ${RIPR}, %xmm0`],
     ['i8x16.max_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f de 05 ${RIPRADDR}   pmaxubx ${RIPR}, %xmm0`],
     ['i8x16.eq', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 74 05 ${RIPRADDR}   pcmpeqbx ${RIPR}, %xmm0`],
     ['i8x16.ne', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
66 0f 74 05 ${RIPRADDR}   pcmpeqbx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i8x16.gt_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 64 05 ${RIPRADDR}   pcmpgtbx ${RIPR}, %xmm0`],
     ['i8x16.le_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
66 0f 64 05 ${RIPRADDR}   pcmpgtbx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i8x16.narrow_i16x8_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 63 05 ${RIPRADDR}   packsswbx ${RIPR}, %xmm0`],
     ['i8x16.narrow_i16x8_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 67 05 ${RIPRADDR}   packuswbx ${RIPR}, %xmm0`],

     ['i16x8.add', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f fd 05 ${RIPRADDR}   paddwx ${RIPR}, %xmm0`],
     ['i16x8.sub', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f f9 05 ${RIPRADDR}   psubwx ${RIPR}, %xmm0`],
     ['i16x8.mul', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f d5 05 ${RIPRADDR}   pmullwx ${RIPR}, %xmm0`],
     ['i16x8.add_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ed 05 ${RIPRADDR}   paddswx ${RIPR}, %xmm0`],
     ['i16x8.add_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f dd 05 ${RIPRADDR}   padduswx ${RIPR}, %xmm0`],
     ['i16x8.sub_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f e9 05 ${RIPRADDR}   psubswx ${RIPR}, %xmm0`],
     ['i16x8.sub_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f d9 05 ${RIPRADDR}   psubuswx ${RIPR}, %xmm0`],
     ['i16x8.min_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ea 05 ${RIPRADDR}   pminswx ${RIPR}, %xmm0`],
     ['i16x8.min_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 38 3a 05 ${RIPRADDR} pminuwx ${RIPR}, %xmm0`],
     ['i16x8.max_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ee 05 ${RIPRADDR}   pmaxswx ${RIPR}, %xmm0`],
     ['i16x8.max_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 38 3e 05 ${RIPRADDR} pmaxuwx ${RIPR}, %xmm0`],
     ['i16x8.eq', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 75 05 ${RIPRADDR}   pcmpeqwx ${RIPR}, %xmm0`],
     ['i16x8.ne', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
66 0f 75 05 ${RIPRADDR}   pcmpeqwx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i16x8.gt_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 65 05 ${RIPRADDR}   pcmpgtwx ${RIPR}, %xmm0`],
     ['i16x8.le_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
66 0f 65 05 ${RIPRADDR}   pcmpgtwx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i16x8.narrow_i32x4_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 6b 05 ${RIPRADDR}   packssdwx ${RIPR}, %xmm0`],
     ['i16x8.narrow_i32x4_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 38 2b 05 ${RIPRADDR} packusdwx ${RIPR}, %xmm0`],

     ['i32x4.add', '(v128.const i32x4 1 2 1 2)',
      `66 0f fe 05 ${RIPRADDR}   padddx ${RIPR}, %xmm0`],
     ['i32x4.sub', '(v128.const i32x4 1 2 1 2)',
      `66 0f fa 05 ${RIPRADDR}   psubdx ${RIPR}, %xmm0`],
     ['i32x4.mul', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 40 05 ${RIPRADDR} pmulldx ${RIPR}, %xmm0`],
     ['i32x4.min_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 39 05 ${RIPRADDR} pminsdx ${RIPR}, %xmm0`],
     ['i32x4.min_u', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3b 05 ${RIPRADDR} pminudx ${RIPR}, %xmm0`],
     ['i32x4.max_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3d 05 ${RIPRADDR} pmaxsdx ${RIPR}, %xmm0`],
     ['i32x4.max_u', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3f 05 ${RIPRADDR} pmaxudx ${RIPR}, %xmm0`],
     ['i32x4.eq', '(v128.const i32x4 1 2 1 2)',
      `66 0f 76 05 ${RIPRADDR}   pcmpeqdx ${RIPR}, %xmm0`],
     ['i32x4.ne', '(v128.const i32x4 1 2 1 2)', `
66 0f 76 05 ${RIPRADDR}   pcmpeqdx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i32x4.gt_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f 66 05 ${RIPRADDR}   pcmpgtdx ${RIPR}, %xmm0`],
     ['i32x4.le_s', '(v128.const i32x4 1 2 1 2)', `
66 0f 66 05 ${RIPRADDR}   pcmpgtdx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i32x4.dot_i16x8_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f f5 05 ${RIPRADDR}   pmaddwdx ${RIPR}, %xmm0`],

     ['i64x2.add', '(v128.const i64x2 1 2)',
      `66 0f d4 05 ${RIPRADDR}   paddqx ${RIPR}, %xmm0`],
     ['i64x2.sub', '(v128.const i64x2 1 2)',
      `66 0f fb 05 ${RIPRADDR}   psubqx ${RIPR}, %xmm0`],

     ['v128.and', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f db 05 ${RIPRADDR}   pandx ${RIPR}, %xmm0`],
     ['v128.or', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f eb 05 ${RIPRADDR}   porx ${RIPR}, %xmm0`],
     ['v128.xor', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f ef 05 ${RIPRADDR}   pxorx ${RIPR}, %xmm0`],

     ['f32x4.add', '(v128.const f32x4 1 2 3 4)',
      `0f 58 05 ${RIPRADDR}      addpsx ${RIPR}, %xmm0`],
     ['f32x4.sub', '(v128.const f32x4 1 2 3 4)',
      `0f 5c 05 ${RIPRADDR}      subpsx ${RIPR}, %xmm0`],
     ['f32x4.mul', '(v128.const f32x4 1 2 3 4)',
      `0f 59 05 ${RIPRADDR}      mulpsx ${RIPR}, %xmm0`],
     ['f32x4.div', '(v128.const f32x4 1 2 3 4)',
      `0f 5e 05 ${RIPRADDR}      divpsx ${RIPR}, %xmm0`],
     ['f32x4.eq', '(v128.const f32x4 1 2 3 4)',
      `0f c2 05 ${RIPRADDR} 00   cmppsx \\$0x00, ${RIPR}, %xmm0`],
     ['f32x4.ne', '(v128.const f32x4 1 2 3 4)',
      `0f c2 05 ${RIPRADDR} 04   cmppsx \\$0x04, ${RIPR}, %xmm0`],
     ['f32x4.lt', '(v128.const f32x4 1 2 3 4)',
      `0f c2 05 ${RIPRADDR} 01   cmppsx \\$0x01, ${RIPR}, %xmm0`],
     ['f32x4.le', '(v128.const f32x4 1 2 3 4)',
      `0f c2 05 ${RIPRADDR} 02   cmppsx \\$0x02, ${RIPR}, %xmm0`],

     ['f64x2.add', '(v128.const f64x2 1 2)',
      `66 0f 58 05 ${RIPRADDR}      addpdx ${RIPR}, %xmm0`],
     ['f64x2.sub', '(v128.const f64x2 1 2)',
      `66 0f 5c 05 ${RIPRADDR}      subpdx ${RIPR}, %xmm0`],
     ['f64x2.mul', '(v128.const f64x2 1 2)',
      `66 0f 59 05 ${RIPRADDR}      mulpdx ${RIPR}, %xmm0`],
     ['f64x2.div', '(v128.const f64x2 1 2)',
      `66 0f 5e 05 ${RIPRADDR}      divpdx ${RIPR}, %xmm0`],
     ['f64x2.eq', '(v128.const f64x2 1 2)',
      `66 0f c2 05 ${RIPRADDR} 00   cmppdx \\$0x00, ${RIPR}, %xmm0`],
     ['f64x2.ne', '(v128.const f64x2 1 2)',
      `66 0f c2 05 ${RIPRADDR} 04   cmppdx \\$0x04, ${RIPR}, %xmm0`],
     ['f64x2.lt', '(v128.const f64x2 1 2)',
      `66 0f c2 05 ${RIPRADDR} 01   cmppdx \\$0x01, ${RIPR}, %xmm0`],
     ['f64x2.le', '(v128.const f64x2 1 2)',
      `66 0f c2 05 ${RIPRADDR} 02   cmppdx \\$0x02, ${RIPR}, %xmm0`]]);

// Commutative operations with constants on the lhs should generate the same
// code as with the constant on the rhs.

codegenTestX64_LITERALxv128_v128(
    [['i8x16.add', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f fc 05 ${RIPRADDR}   paddbx ${RIPR}, %xmm0`],
     ['i8x16.add_sat_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f ec 05 ${RIPRADDR}   paddsbx ${RIPR}, %xmm0`],
     ['i8x16.add_sat_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f dc 05 ${RIPRADDR}   paddusbx ${RIPR}, %xmm0`],
     ['i8x16.min_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 38 38 05 ${RIPRADDR} pminsbx ${RIPR}, %xmm0`],
     ['i8x16.min_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f da 05 ${RIPRADDR}   pminubx ${RIPR}, %xmm0`],
     ['i8x16.max_s', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 38 3c 05 ${RIPRADDR} pmaxsbx ${RIPR}, %xmm0`],
     ['i8x16.max_u', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f de 05 ${RIPRADDR}   pmaxubx ${RIPR}, %xmm0`],
     ['i8x16.eq', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f 74 05 ${RIPRADDR}   pcmpeqbx ${RIPR}, %xmm0`],
     ['i8x16.ne', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)', `
66 0f 74 05 ${RIPRADDR}   pcmpeqbx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],

     ['i16x8.add', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f fd 05 ${RIPRADDR}   paddwx ${RIPR}, %xmm0`],
     ['i16x8.mul', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f d5 05 ${RIPRADDR}   pmullwx ${RIPR}, %xmm0`],
     ['i16x8.add_sat_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ed 05 ${RIPRADDR}   paddswx ${RIPR}, %xmm0`],
     ['i16x8.add_sat_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f dd 05 ${RIPRADDR}   padduswx ${RIPR}, %xmm0`],
     ['i16x8.min_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ea 05 ${RIPRADDR}   pminswx ${RIPR}, %xmm0`],
     ['i16x8.min_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 38 3a 05 ${RIPRADDR} pminuwx ${RIPR}, %xmm0`],
     ['i16x8.max_s', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f ee 05 ${RIPRADDR}   pmaxswx ${RIPR}, %xmm0`],
     ['i16x8.max_u', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 38 3e 05 ${RIPRADDR} pmaxuwx ${RIPR}, %xmm0`],
     ['i16x8.eq', '(v128.const i16x8 1 2 1 2 1 2 1 2)',
      `66 0f 75 05 ${RIPRADDR}   pcmpeqwx ${RIPR}, %xmm0`],
     ['i16x8.ne', '(v128.const i16x8 1 2 1 2 1 2 1 2)', `
66 0f 75 05 ${RIPRADDR}   pcmpeqwx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],

     ['i32x4.add', '(v128.const i32x4 1 2 1 2)',
      `66 0f fe 05 ${RIPRADDR}   padddx ${RIPR}, %xmm0`],
     ['i32x4.mul', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 40 05 ${RIPRADDR} pmulldx ${RIPR}, %xmm0`],
     ['i32x4.min_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 39 05 ${RIPRADDR} pminsdx ${RIPR}, %xmm0`],
     ['i32x4.min_u', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3b 05 ${RIPRADDR} pminudx ${RIPR}, %xmm0`],
     ['i32x4.max_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3d 05 ${RIPRADDR} pmaxsdx ${RIPR}, %xmm0`],
     ['i32x4.max_u', '(v128.const i32x4 1 2 1 2)',
      `66 0f 38 3f 05 ${RIPRADDR} pmaxudx ${RIPR}, %xmm0`],
     ['i32x4.eq', '(v128.const i32x4 1 2 1 2)',
      `66 0f 76 05 ${RIPRADDR}   pcmpeqdx ${RIPR}, %xmm0`],
     ['i32x4.ne', '(v128.const i32x4 1 2 1 2)', `
66 0f 76 05 ${RIPRADDR}   pcmpeqdx ${RIPR}, %xmm0
66 45 0f 75 ff            pcmpeqw %xmm15, %xmm15
66 41 0f ef c7            pxor %xmm15, %xmm0`],
     ['i32x4.dot_i16x8_s', '(v128.const i32x4 1 2 1 2)',
      `66 0f f5 05 ${RIPRADDR}   pmaddwdx ${RIPR}, %xmm0`],

     ['i64x2.add', '(v128.const i64x2 1 2)',
      `66 0f d4 05 ${RIPRADDR}   paddqx ${RIPR}, %xmm0`],

     ['v128.and', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f db 05 ${RIPRADDR}   pandx ${RIPR}, %xmm0`],
     ['v128.or', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f eb 05 ${RIPRADDR}   porx ${RIPR}, %xmm0`],
     ['v128.xor', '(v128.const i8x16 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 2)',
      `66 0f ef 05 ${RIPRADDR}   pxorx ${RIPR}, %xmm0`]]);
