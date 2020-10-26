// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves or other instructions for splat and
// other splat-like operations that can reuse its input for its output and/or
// has a specializable code path.  See README-codegen.md for general information
// about this type of test case.

// Input (xmm0)

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
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (func (export "f") (param ${type}) (result v128)
      (${simd_type}.splat (local.get 0))))`)));
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

// Input (paramreg0)

for ( let [ op, expected ] of [
    ['v128.load32_splat', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f3 41 0f 10 04 3f         movssl \\(%r15,%rdi,1\\), %xmm0
000000..  0f c6 c0 00               shufps \\$0x00, %xmm0, %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load64_splat', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  f2 41 0f 12 04 3f         movddupq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load8x8_s', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 20 04 3f      pmovsxbwq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load8x8_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 30 04 3f      pmovzxbwq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load16x4_s', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 23 04 3f      pmovsxwdq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load16x4_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 33 04 3f      pmovzxwdq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load32x2_s', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 25 04 3f      pmovsxdqq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
    ['v128.load32x2_u', `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 41 0f 38 35 04 3f      pmovzxdqq \\(%r15,%rdi,1\\), %xmm0
000000..  5d                        pop %rbp
`],
] ) {

    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
  (module
    (memory 1)
    (func (export "f") (param i32) (result v128)
      (${op} (local.get 0))))`)));
    let output = wasmDis(ins.exports.f, "ion", true);
    if (output.indexOf('No disassembly available') >= 0)
        continue;
    assertEq(output.match(new RegExp(expected)) != null, true);
}

