// |jit-test| skip-if: wasmGcEnabled()

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|(Structure|reference|gc) types not enabled|invalid heap type|invalid inline block type|bad type|\(ref T\) types not enabled|Cranelift error in clifFunc/;

let simpleTests = [
    "(module (func (drop (ref.null any))))",
    "(module (func $test (local anyref)))",
    "(module (func $test (param anyref)))",
    "(module (func $test (result anyref) (ref.null any)))",
    "(module (func $test (block (result anyref) (unreachable)) unreachable))",
    "(module (func $test (result i32) (local anyref) (ref.is_null (local.get 0))))",
    `(module (import "a" "b" (func (param anyref))))`,
    `(module (import "a" "b" (func (result anyref))))`,
    `(module (type $s (struct)))`,
    `(module (func (param (ref 0)) (unreachable)))`,
];

// Test that use of gc-types fails when gc is disabled.

for (let src of simpleTests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
}
