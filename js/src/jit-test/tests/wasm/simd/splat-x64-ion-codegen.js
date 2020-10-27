// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves for a variable float splat that can reuse
// its input for its output.  See README-codegen.md for general
// information about this type of test case.

// Inputs (xmm0, xmm1)

for ( let [ simd_type, expected ] of [
    ['f32x4', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  0f c6 c0 00               shufps \\$0x00, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['f64x2', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f c6 c0 00            shufpd \\$0x00, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
] ) {

    let type = simd_type.substring(0,3);
    let ins = wasmEvalText(`
  (module
    (func (export "f") (param ${type}) (result v128)
      (${simd_type}.splat (local.get 0))))`);
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

