wasmFailValidateText(`(module
  (func
    throw_ref
  )
)`, /popping value from empty stack/);

wasmValidateText(`(module
  (func (param exnref)
    local.get 0
    throw_ref
  )
)`);

// Can rethrow a value
{
  let {test} = wasmEvalText(`(module
    (tag $a)
    (func (export "test")
      try_table (result exnref) (catch_all_ref 0)
        throw $a
        unreachable
      end
      throw_ref
    )
  )`).exports;

  assertErrorMessage(test, WebAssembly.Exception, /.*/);
}

// Traps on null
{
  let {test} = wasmEvalText(`(module
    (tag $a)
    (func (export "test")
      ref.null exn
      throw_ref
    )
  )`).exports;

  assertErrorMessage(test, WebAssembly.RuntimeError, /null/);
}
