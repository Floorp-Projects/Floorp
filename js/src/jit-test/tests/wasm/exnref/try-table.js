// A try_table acts like a block label
{
  let {test} = wasmEvalText(`(module
    (func (export "test") (result i32)
      try_table
        br 0
        (return i32.const 0)
      end
      (return i32.const 1)
    )
  )`).exports;
  assertEq(test(), 1);
}

// A try_table can have results
{
  let {test} = wasmEvalText(`(module
    (func (export "test") (result i32)
      try_table (result i32)
        i32.const 1
        br 0
      end
    )
  )`).exports;
  assertEq(test(), 1);
}

// A try_table can have params
{
  let {test} = wasmEvalText(`(module
    (func (export "test") (result i32)
      i32.const 1
      try_table (param i32)
        return
      end
      (return i32.const 0)
    )
  )`).exports;
  assertEq(test(), 1);
}

// Test try_table catching exceptions
{
  let {test} = wasmEvalText(`(module
    (tag $A (param i32))
    (tag $B (param i32))
    (tag $C)

    (table funcref (elem $throwA $throwB $throwC $doNothing))

    (type $empty (func))
    (func $throwA
      i32.const 1
      throw $A
    )
    (func $throwB
      i32.const 2
      throw $B
    )
    (func $throwC
      throw $C
    )
    (func $doNothing)

    (func (export "test") (param i32) (result i32)
      block $handleA (result i32 exnref)
        block $handleB (result i32 exnref)
          block $handleUnknown (result exnref)
            try_table
              (catch_ref $A $handleA)
              (catch_ref $B $handleB)
              (catch_all_ref $handleUnknown)

              (call_indirect (type $empty)
                local.get 0)
            end
            (; nothing threw ;)
            i32.const -1
            return
          end
          (; $handleUnknown ;)
          drop
          i32.const 3
          return
        end
         (; $handleB ;)
        drop
        return
      end
      (; $handleA ;)
      drop
      return
    )
  )`).exports;
  // Throwing A results in 1 from the payload from the catch
  assertEq(test(0), 1);
  // Throwing B results in 2 from the payload from the catch
  assertEq(test(1), 2);
  // Throwing C results in 3 from the constant in the catch_all
  assertEq(test(2), 3);
  // Not throwing anything gets -1 from the fallthrough
  assertEq(test(3), -1);
}

// Test try_table catching exceptions without capturing the exnref
{
  let {test} = wasmEvalText(`(module
    (tag $A (param i32))
    (tag $B (param i32))
    (tag $C)

    (table funcref (elem $throwA $throwB $throwC $doNothing))

    (type $empty (func))
    (func $throwA
      i32.const 1
      throw $A
    )
    (func $throwB
      i32.const 2
      throw $B
    )
    (func $throwC
      throw $C
    )
    (func $doNothing)

    (func (export "test") (param i32) (result i32)
      block $handleA (result i32)
        block $handleB (result i32)
          block $handleUnknown
            try_table
              (catch $A $handleA)
              (catch $B $handleB)
              (catch_all $handleUnknown)

              (call_indirect (type $empty)
                local.get 0)
            end
            (; nothing threw ;)
            i32.const -1
            return
          end
          (; $handleUnknown ;)
          i32.const 3
          return
        end
         (; $handleB ;)
        return
      end
      (; $handleA ;)
      return
    )
  )`).exports;
  // Throwing A results in 1 from the payload from the catch
  assertEq(test(0), 1);
  // Throwing B results in 2 from the payload from the catch
  assertEq(test(1), 2);
  // Throwing C results in 3 from the constant in the catch_all
  assertEq(test(2), 3);
  // Not throwing anything gets -1 from the fallthrough
  assertEq(test(3), -1);
}

// Test try_table catching and rethrowing JS exceptions
{
  let tag = new WebAssembly.Tag({parameters: []});
  let exn = new WebAssembly.Exception(tag, []);
  let values = [...WasmExternrefValues, exn];
  function throwJS(value) {
    throw value;
  }
  let {test} = wasmEvalText(`(module
    (import "" "tag" (tag $tag))
    (import "" "throwJS" (func $throwJS (param externref)))
    (func (export "test") (param externref)
      try_table (result exnref) (catch_ref $tag 0) (catch_all_ref 0)
        local.get 0
        call $throwJS
        return
      end
      throw_ref
    )
  )`, {"": {tag, throwJS}}).exports;

  for (let value of values) {
    // TODO: A JS null value should become a non-null exnref that can be
    // rethrown without a trap.
    if (value === null) {
      continue;
    }

    try {
      test(value);
      assertEq(true, false);
    } catch (thrownValue) {
      assertEq(thrownValue, value);
    }
  }
}
