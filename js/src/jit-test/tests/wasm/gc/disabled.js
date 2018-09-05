if (wasmGcEnabled()) {
    quit();
}

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|(Structure|reference) types not enabled|invalid inline block type/;

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
    `(module (gc_feature_opt_in 1) (type $s (struct)))`,
];

// Two distinct failure modes:
//
// - if we have no compiled-in support for wasm-gc we'll get a syntax error when
//   parsing the test programs that use ref types and structures.
//
// - if we have compiled-in support for wasm-gc, but wasm-gc is not currently
//   enabled, we will succeed parsing but fail compilation and validation.
//
// But it should always be all of one or all of the other.

var fail_syntax = 0;
var fail_compile = 0;
for (let src of simpleTests) {
    try {
        wasmTextToBinary(src);
    } catch (e) {
        assertEq(e instanceof SyntaxError, true);
        fail_syntax++;
        continue;
    }
    assertErrorMessage(() => wasmEvalText(src), CompileError, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
    assertValidateError(src);
    fail_compile++;
}
assertEq((fail_syntax == 0) != (fail_compile == 0), true);
