// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator"); include:codegen-x64-test.js

// Tests for SIMD add pairwise instructions.

if (!isAvxPresent()) {

     codegenTestX64_IGNOREDxv128_v128(
          [['i16x8.extadd_pairwise_i8x16_s', `
66 0f 6f 05 ${RIPRADDR}    movdqax ${RIPR}, %xmm0
66 0f 38 04 c1             pmaddubsw %xmm1, %xmm0`],
           ['i16x8.extadd_pairwise_i8x16_u', `
66 0f 6f c1                movdqa %xmm1, %xmm0
66 0f 38 04 05 ${RIPRADDR} pmaddubswx ${RIPR}, %xmm0`],
           ['i32x4.extadd_pairwise_i16x8_s', `
66 0f 6f c1                movdqa %xmm1, %xmm0
66 0f f5 05 ${RIPRADDR}    pmaddwdx ${RIPR}, %xmm0`],
           ['i32x4.extadd_pairwise_i16x8_u', `
66 0f 6f c1                movdqa %xmm1, %xmm0
66 0f ef 05 ${RIPRADDR}    pxorx ${RIPR}, %xmm0
66 0f f5 05 ${RIPRADDR}    pmaddwdx ${RIPR}, %xmm0
66 0f fe 05 ${RIPRADDR}    padddx ${RIPR}, %xmm0`]]);

} else {

     codegenTestX64_IGNOREDxv128_v128(
          [['i16x8.extadd_pairwise_i8x16_s', `
66 0f 6f 05 ${RIPRADDR}    movdqax ${RIPR}, %xmm0
66 0f 38 04 c1             pmaddubsw %xmm1, %xmm0`],
           ['i16x8.extadd_pairwise_i8x16_u', `
c4 e2 71 04 05 ${RIPRADDR} vpmaddubswx ${RIPR}, %xmm1, %xmm0`],
           ['i32x4.extadd_pairwise_i16x8_s', `
c5 f1 f5 05 ${RIPRADDR}    vpmaddwdx ${RIPR}, %xmm1, %xmm0`],
           ['i32x4.extadd_pairwise_i16x8_u', `
c5 f1 ef 05 ${RIPRADDR}    vpxorx ${RIPR}, %xmm1, %xmm0
66 0f f5 05 ${RIPRADDR}    pmaddwdx ${RIPR}, %xmm0
66 0f fe 05 ${RIPRADDR}    padddx ${RIPR}, %xmm0`]]);

}
