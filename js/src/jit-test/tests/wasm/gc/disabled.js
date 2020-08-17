// |jit-test| skip-if: wasmReftypesEnabled() || wasmGcEnabled()

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|(Structure|reference) types not enabled|invalid inline block type|bad type|Cranelift error in clifFunc/;

let simpleTests = [
    "(module (func (drop (ref.null extern))))",
    "(module (func $test (local externref)))",
    "(module (func $test (param externref)))",
    "(module (func $test (result externref) (ref.null extern)))",
    "(module (func $test (block (result externref) (unreachable)) unreachable))",
    "(module (func $test (result i32) (local externref) (ref.is_null (local.get 0))))",
    `(module (import "a" "b" (func (param externref))))`,
    `(module (import "a" "b" (func (result externref))))`,
    `(module (type $s (struct)))`,
];

// Test that use of reference-types or structs fails when
// reference-types is disabled.

for (let src of simpleTests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
}
