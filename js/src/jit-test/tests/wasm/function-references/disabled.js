// |jit-test| skip-if: wasmFunctionReferencesEnabled()

const { CompileError, validate } = WebAssembly;

const UNRECOGNIZED_OPCODE_OR_BAD_TYPE = /unrecognized opcode|bad type|\(ref T\) types not enabled/;

let simpleTests = [
    `(module (func (param (ref 0)) (unreachable)))`,
];

// Test that use of function-references fails when function-references is disabled.

for (let src of simpleTests) {
    let bin = wasmTextToBinary(src);
    assertEq(validate(bin), false);
    wasmCompilationShouldFail(bin, UNRECOGNIZED_OPCODE_OR_BAD_TYPE);
}
