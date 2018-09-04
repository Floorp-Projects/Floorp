if (wasmGcEnabled()) {
    quit();
}

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|reference types not enabled|invalid inline block type/;

function assertValidateError(text) {
    assertEq(validate(wasmTextToBinary(text)), false);
}

let simpleTests = [
    "(module (gc_feature_opt_in 1) (func (drop (ref.null anyref))))",
    "(module (gc_feature_opt_in 1) (func $test (local anyref)))",
    "(module (gc_feature_opt_in 1) (func $test (param anyref)))",
    "(module (gc_feature_opt_in 1) (func $test (result anyref) (ref.null anyref)))",
    "(module (gc_feature_opt_in 1) (func $test (block anyref (unreachable)) unreachable))",
    "(module (gc_feature_opt_in 1) (func $test (local anyref) (result i32) (ref.is_null (get_local 0))))",
    `(module (gc_feature_opt_in 1) (import "a" "b" (param anyref)))`,
    `(module (gc_feature_opt_in 1) (import "a" "b" (result anyref)))`,
];

for (let src of simpleTests) {
    print(src)
    assertErrorMessage(() => wasmEvalText(src), CompileError, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
    assertValidateError(src);
}
