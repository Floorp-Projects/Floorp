// |jit-test| skip-if: !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator; include:codegen-x64-test.js

// Test that multiplication by -1 yields negation.  There may be some redundant
// moves to set that up (that's bug 1701164) so avoid those with
// no_prefix/no_suffix.

let neg32 =
    `(module
       (func (export "f") (param i32) (result i32)
         (i32.mul (local.get 0) (i32.const -1))))`;
codegenTestX64_adhoc(
    neg32,
    'f',
    'f7 d8  neg %eax', {no_prefix:true, no_suffix:true});
assertEq(wasmEvalText(neg32).exports.f(-37), 37)
assertEq(wasmEvalText(neg32).exports.f(42), -42)

let neg64 =
    `(module
       (func (export "f") (param i64) (result i64)
         (i64.mul (local.get 0) (i64.const -1))))`;
codegenTestX64_adhoc(
    neg64,
    'f',
    '48 f7 d8  neg %rax', {no_prefix:true, no_suffix:true});
assertEq(wasmEvalText(neg64).exports.f(-37000000000n), 37000000000n)
assertEq(wasmEvalText(neg64).exports.f(42000000000n), -42000000000n)
