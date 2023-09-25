// |jit-test| --wasm-compiler=optimizing; --disable-wasm-huge-memory; --spectre-mitigations=off; skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator") || getJitCompilerOptions()["ion.check-range-analysis"]; include:codegen-x64-test.js

// Spectre mitigation is disabled above to make the generated code simpler to
// match; ion.check-range-analysis makes a hash of the code and makes testing
// pointless.

// White-box testing of bounds check elimination on 64-bit systems.
//
// This is probably fairly brittle, but BCE on 64-bit platforms regressed (bug
// 1735207) without us noticing, so it's not like we can do without these tests.
//
// See also bce-x86-ion-codegen.js.
//
// If this turns out to be too brittle to be practical then an alternative to
// testing the output code is to do what we do for SIMD, record (in a log or
// buffer of some kind) that certain optimizations triggered, and then check the
// log.

var memTypes = [''];
if (wasmMemory64Enabled()) {
    memTypes.push('i64')
}

for ( let memType of memTypes ) {
    let dataType = memType ? memType : 'i32';

    // Make sure the check for the second load is removed: the two load
    // instructions should appear back-to-back in the output.
    codegenTestX64_adhoc(
`(module
   (memory ${memType} 1)
   (func (export "f") (param ${dataType}) (result i32)
     (local ${dataType})
     (local.set 1 (${dataType}.add (local.get 0) (${dataType}.const 8)))
     (i32.load (local.get 1))
     drop
     (i32.load (local.get 1))))`,
    'f', `
48 3b ..                  cmp %r.., %r..
0f 83 .. 00 00 00         jnb 0x00000000000000..
41 8b .. ..               movl \\(%r15,%r..,1\\), %e..
41 8b .. ..               movl \\(%r15,%r..,1\\), %eax`,
        {no_prefix:true});

    // Make sure constant indices below the heap minimum do not require a bounds
    // check.
    codegenTestX64_adhoc(
`(module
   (memory ${memType} 1)
   (func (export "f") (result i32)
     (i32.load (${dataType}.const 16))))`,
    'f',
    `41 8b 47 10               movl 0x10\\(%r15\\), %eax`);

    // Ditto, even at the very limit of the known heap, extending into the guard
    // page.  This is an OOB access, of course, but it needs no explicit bounds
    // check.
    codegenTestX64_adhoc(
`(module
   (memory ${memType} 1)
   (func (export "f") (result i32)
     (i32.load (${dataType}.const 65535))))`,
    'f',
`
b8 ff ff 00 00            mov \\$0xFFFF, %eax
41 8b 04 07               movl \\(%r15,%rax,1\\), %eax`);
}

