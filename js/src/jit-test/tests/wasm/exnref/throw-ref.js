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
      (block (result exnref)
        try_table (catch_all_ref 0)
          throw $a
        end
        unreachable
      )
      throw_ref
    )
  )`).exports;

  assertErrorMessage(test, WebAssembly.Exception, /.*/);
}

// Rethrowing a value inside a try works
{
  let {test} = wasmEvalText(`(module
    (tag $E)
    (func (export "test") (param $shouldRethrow i32) (result i32)
      (local $e exnref)
      (block $catch (result exnref)
        (try_table (catch_ref $E $catch) (throw $E))
        unreachable
      )
      (local.set $e)
      (block $catch (result exnref)
        (try_table (result i32) (catch_ref $E $catch)
          (if (i32.eqz (local.get $shouldRethrow))
            (then (throw_ref (local.get $e)))
          )
          (i32.const 2)
        )
        (return)
      )
      (drop) (i32.const 1)
    )
  )`).exports;
  assertEq(test(0), 1);
  assertEq(test(1), 2);
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
