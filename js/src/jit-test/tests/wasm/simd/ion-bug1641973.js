// |jit-test| skip-if: !wasmSimdSupported()

// Fuzz test case.  The initial unreachable will result in the subsequent
// v8x16.shuffle popping null pointers off the value stack.  Due to a missing
// isDeadCode() check in WasmIonCompile.cpp the compiler would dereference those
// null pointers.
new WebAssembly.Module(wasmTextToBinary(`
(module
  (func (result v128)
    (unreachable)
    (v8x16.shuffle 0 0 23 0 4 4 4 4 4 16 1 0 4 4 4 4)))
`))

