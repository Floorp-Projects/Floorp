// |jit-test| skip-if: !wasmGcEnabled()

let { createDefault } = wasmEvalText(`
  (module (type $a (array (mut i32)))
    (func (export "createDefault") (param i32) (result eqref)
      local.get 0
      rtt.canon $a 
      array.new_default_with_rtt $a     
    )
  )
`).exports;
for (len = -1; len > -100; len--) {
  assertErrorMessage(() => createDefault(len), WebAssembly.RuntimeError, /too many array elements/);
}
