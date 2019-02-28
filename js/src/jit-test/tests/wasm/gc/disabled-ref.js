// |jit-test| skip-if: wasmGcEnabled()

wasmCompilationShouldFail(
    wasmTextToBinary(`(module (func (param (ref 0)) (unreachable)))`),
    /reference types not enabled/);
