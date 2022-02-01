let {shouldTrap} = wasmEvalText(`(module
  (func
    try
      i32.const 0
      table.get 0
      ref.is_null
      drop
    catch_all
    end
  )
  (table 0 funcref)
  (export "shouldTrap" (func 0))
)`).exports;
assertErrorMessage(shouldTrap, WebAssembly.RuntimeError, /table index/);
