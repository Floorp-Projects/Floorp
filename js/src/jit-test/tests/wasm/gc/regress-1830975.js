// |jit-test| skip-if: !wasmGcEnabled()

// Hello, future wasm dev. This test will start failing when you add casting
// for func and extern types. When that happens, remove the assertErrorMessage.

assertErrorMessage(() => {
  const { test } = wasmEvalText(`
    (module
      (type $f (func (result i32)))
      (func (export "test") (type $f)
        ref.null $f
        ref.test (ref null $f)
      )
    )
  `).exports;
  assertEq(test(), 1);
}, WebAssembly.CompileError, /ref.test only supports the any hierarchy/);
