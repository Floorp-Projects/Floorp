// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration("x64") || getBuildConfiguration("simulator") || isAvxPresent(); include:codegen-x64-test.js

// This checks that we emit a REX prefix that includes the SIB index when
// appropriate.
//
// This test case is a little tricky.  On Win64, the arg registers are rcx, rdx,
// r8, r9; so we want to use local 2 or 3 as the index.  But on other x64
// platforms, the arg registers are rdi, rsi, rdx, rcx, r8, r9; so we want to
// use local 4 or 5 as the index.  This test uses both, and then looks for a hit
// on the REX byte which must be 0x43.  Before the bugfix, since the index
// register was ignored, the byte would always be 0x41, as it will continue to
// be for the access that does not use an extended register.
//
// The test is brittle: the register allocator can easily make a mess of it.
// But for now it works.

codegenTestX64_adhoc(
`(module
   (memory 1)
   (func $f (export "f") (param i32) (param i32) (param i32) (param i32) (param i32) (result v128)
     (i32x4.add (v128.load8x8_s (local.get 4)) (v128.load8x8_s (local.get 2)))))`,
    'f',
    `66 43 0f 38 20 .. ..      pmovsxbwq \\(%r15,%r(8|9|10|11|12|13),1\\), %xmm[0-9]+`,
    {no_prefix: true, no_suffix: true, log:true});
