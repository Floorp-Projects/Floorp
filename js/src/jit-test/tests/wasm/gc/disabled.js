if (wasmGcEnabled()) {
    quit();
}

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /(unrecognized opcode|bad type|invalid inline block type)/;

function assertValidateError(text) {
    assertEq(validate(wasmTextToBinary(text)), false);
}

let simpleTests = [
    "(module (func (drop (ref.null anyref))))",
    "(module (func $test (local anyref)))",
    "(module (func $test (param anyref)))",
    "(module (func $test (result anyref) (ref.null anyref)))",
    "(module (func $test (block anyref (unreachable)) unreachable))",
    "(module (func $test (local anyref) (result i32) (ref.is_null (get_local 0))))",
    `(module (import "a" "b" (param anyref)))`,
    `(module (import "a" "b" (result anyref)))`,
];

for (let src of simpleTests) {
    print(src)
    assertErrorMessage(() => wasmEvalText(src), CompileError, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
    assertValidateError(src);
}
