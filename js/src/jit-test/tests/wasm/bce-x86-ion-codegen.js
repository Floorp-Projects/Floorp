// |jit-test| --wasm-compiler=optimizing; --spectre-mitigations=off; skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x86") || getBuildConfiguration("simulator") || getJitCompilerOptions()["ion.check-range-analysis"]; include:codegen-x86-test.js

// Spectre mitigation is disabled above to make the generated code simpler to
// match; ion.check-range-analysis makes a hash of the code and makes testing
// pointless.

// White-box testing of bounds check elimination on 32-bit systems.
//
// This is probably fairly brittle, but BCE (on 64-bit platforms) regressed (bug
// 1735207) without us noticing, so it's not like we can do without these tests.
//
// See also bce-x64-ion-codegen.js.

// Make sure the check for the second load is removed: the two load instructions
// should appear back-to-back in the output.
codegenTestX86_adhoc(
`(module
   (memory 1)
   (func (export "f") (param i32) (result i32)
     (local i32)
     (local.set 1 (i32.add (local.get 0) (i32.const 4)))
     (i32.load (local.get 1))
     drop
     (i32.load (local.get 1))))`,
    'f', `
3b ..                     cmp %e.., %e..
0f 83 .. 00 00 00         jnb 0x00000000000000..
8b .. ..                  movl \\(%r..,%r..,1\\), %e..
8b .. ..                  movl \\(%r..,%r..,1\\), %eax`,
    {no_prefix:true});

// Make sure constant indices below the heap minimum do not require a bounds check.
// The first movl from *rsi below loads the heap base from Tls, an x86-ism.
codegenTestX86_adhoc(
`(module
   (memory 1)
   (func (export "f") (result i32)
     (i32.load (i32.const 16))))`,
    'f', `
8b ..                     movl \\(%rsi\\), %e..
8b .. 10                  movl 0x10\\(%r..\\), %eax`);

// Ditto, even at the very limit of the known heap, extending into the guard
// page.  This is an OOB access, of course, but it needs no explicit bounds
// check.
codegenTestX86_adhoc(
`(module
   (memory 1)
   (func (export "f") (result i32)
     (i32.load (i32.const 65535))))`,
    'f',
`
8b ..                     movl \\(%rsi\\), %e..
8b .. ff ff 00 00         movl 0xFFFF\\(%r..\\), %eax`);
