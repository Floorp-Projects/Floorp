// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that constants that can be synthesized are synthesized.  See README-codegen.md
// for general information about this type of test case.

// Inputs (xmm0, xmm1)

for ( let [op, expected] of [
    ['v128.const i8x16 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i8x16 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1 -1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c0               pcmpeqw %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i16x8 0 0 0 0 0 0 0 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i16x8 -1 -1 -1 -1 -1 -1 -1 -1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c0               pcmpeqw %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i32x4 0 0 0 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i32x4 -1 -1 -1 -1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c0               pcmpeqw %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i64x2 0 0',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const i64x2 -1 -1',
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 75 c0               pcmpeqw %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const f32x4 0 0 0 0',
     // Arguably this should be xorps but that's for later
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.const f64x2 0 0',
     // Arguably this should be xorpd but that's for later
`
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f ef c0               pxor %xmm0, %xmm0
000000..  5d                        pop %rbp
`],

] ) {
    let ins = wasmEvalText(`
  (module
    (func (export "f") (result v128)
      (${op})))
        `);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}
