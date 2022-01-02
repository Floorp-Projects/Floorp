// |jit-test| skip-if: !wasmFunctionReferencesEnabled()

let {checkNonNull} = wasmEvalText(`(module
  (func (export "checkNonNull") (param externref) (result (ref extern))
    local.get 0
    ref.as_non_null
  )
)`).exports;

assertErrorMessage(() => checkNonNull(null), WebAssembly.RuntimeError, /dereferencing null pointer/);
for (let val of WasmNonNullExternrefValues) {
  assertEq(checkNonNull(val), val, `is non-null`);
}
