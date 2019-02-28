// |jit-test| skip-if: wasmReftypesEnabled()

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|(Structure|reference) types not enabled|invalid inline block type/;

let simpleTests = [
    "(module (gc_feature_opt_in 3) (func (drop (ref.null))))",
    "(module (gc_feature_opt_in 3) (func $test (local anyref)))",
    "(module (gc_feature_opt_in 3) (func $test (param anyref)))",
    "(module (gc_feature_opt_in 3) (func $test (result anyref) (ref.null)))",
    "(module (gc_feature_opt_in 3) (func $test (block anyref (unreachable)) unreachable))",
    "(module (gc_feature_opt_in 3) (func $test (local anyref) (result i32) (ref.is_null (get_local 0))))",
    `(module (gc_feature_opt_in 3) (import "a" "b" (param anyref)))`,
    `(module (gc_feature_opt_in 3) (import "a" "b" (result anyref)))`,
    `(module (gc_feature_opt_in 3) (type $s (struct)))`,
];

// Two distinct failure modes:
//
// - if we have no compiled-in support for wasm-gc we'll get a syntax error when
//   parsing the test programs that use ref types and structures.
//
// - if we have compiled-in support for wasm-gc, then there are several cases
//   encapsulated in wasmCompilationShouldFail().
//
// But it should always be all of one type of failure or or all of the other.

var fail_syntax = 0;
var fail_compile = 0;
for (let src of simpleTests) {
    let bin = null;
    try {
        bin = wasmTextToBinary(src);
    } catch (e) {
        assertEq(e instanceof SyntaxError, true);
        fail_syntax++;
        continue;
    }

    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);

    fail_compile++;
}
assertEq((fail_syntax == 0) != (fail_compile == 0), true);
