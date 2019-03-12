// |jit-test| skip-if: wasmGcEnabled()

wasmCompilationShouldFail(
    wasmTextToBinary(`(module (func (param (ref 0)) (unreachable)))`),
    /\(ref T\) types not enabled|bad type/);
