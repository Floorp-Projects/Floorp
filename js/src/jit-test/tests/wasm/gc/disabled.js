// |jit-test| skip-if: wasmGcEnabled()

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|(Structure|reference|gc) types not enabled|invalid heap type|invalid inline block type|bad type|\(ref T\) types not enabled|Invalid type|invalid function type/;

let simpleTests = [
    "(module (func (drop (ref.null eq))))",
    "(module (func $test (local eqref)))",
    "(module (func $test (param eqref)))",
    "(module (func $test (result eqref) (ref.null eq)))",
    "(module (func $test (block (result eqref) (unreachable)) unreachable))",
    "(module (func $test (result i32) (local eqref) (ref.is_null (local.get 0))))",
    `(module (import "a" "b" (func (param eqref))))`,
    `(module (import "a" "b" (func (result eqref))))`,
    `(module (type $s (struct)))`,
];

// Test that use of gc-types fails when gc is disabled.

for (let src of simpleTests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
}
