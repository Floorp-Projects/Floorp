// |jit-test| skip-if: wasmGcEnabled()

assertErrorMessage(() => wasmEvalText(`(module (func (param (ref 0)) (unreachable)))`),
                   WebAssembly.CompileError, /reference types not enabled/);
