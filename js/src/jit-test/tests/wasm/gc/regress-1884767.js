// |jit-test| skip-if: !wasmGcEnabled()

const { test } = wasmEvalText(`(module
  (type $a (array i32))
  (func (export "test") (result anyref)
    try (result anyref)
      (array.new_default $a (i32.const 999999999))
    catch_all
      unreachable
    end
  )
)`).exports;
assertErrorMessage(() => test(), WebAssembly.RuntimeError, /too many array elements/);
