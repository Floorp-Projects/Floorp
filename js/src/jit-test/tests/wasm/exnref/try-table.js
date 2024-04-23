// A try_table acts like a block label, with results
{
  let maxResults1 = Array.from(Array(1000).keys());
  let maxResults2 = maxResults1.map((x) => x - 1);
  const TESTS = [
    ['i32', 'i32.const', [0], [1]],
    ['i32 '.repeat(1000), 'i32.const', maxResults1, maxResults2],
    ['i64', 'i64.const', [0], [1]],
    ['i64 '.repeat(1000), 'i64.const', maxResults1, maxResults2],
    ['f32', 'f32.const', [0], [1]],
    ['f32 '.repeat(1000), 'f32.const', maxResults1, maxResults2],
    ['f64', 'f64.const', [0], [1]],
    ['f64 '.repeat(1000), 'f64.const', maxResults1, maxResults2],
  ];
  for (let [types, constructor, values1, values2] of TESTS) {
    let {test} = wasmEvalText(`(module
      (func (export "test") (param $shouldBranch i32) (result ${types})
        try_table (result ${types})
          ${values2.map((x) => `${constructor} ${x}`).join(" ")}
          (br_if 1 local.get $shouldBranch)
          ${values1.map((x) => `${constructor} ${x}`).join(" ")}
          br 0
        end
      )
    )`).exports;
    assertEqResults(test(0), values1);
    assertEqResults(test(1), values2);
  }
}

// A try_table can have params
{
  let maxParams1 = Array.from(Array(1000).keys());
  const TESTS = [
    ['i32', 'i32.const', [0]],
    ['i32 '.repeat(1000), 'i32.const', maxParams1],
    ['i64', 'i64.const', [0]],
    ['i64 '.repeat(1000), 'i64.const', maxParams1],
    ['f32', 'f32.const', [0]],
    ['f32 '.repeat(1000), 'f32.const', maxParams1],
    ['f64', 'f64.const', [0]],
    ['f64 '.repeat(1000), 'f64.const', maxParams1],
  ];
  for (let [types, constructor, params] of TESTS) {
    let {test} = wasmEvalText(`(module
      (func (export "test") (result ${types})
        ${params.map((x) => `${constructor} ${x}`).join(" ")}
        try_table (param ${types})
          return
        end
        unreachable
      )
    )`).exports;
    assertEqResults(test(), params);
  }
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

// Test try_table catching exceptions with various payloads
{
  let maxResults1 = Array.from(Array(999).keys());
  const TESTS = [
    ['i32', 'i32.const', [0]],
    ['i32 '.repeat(999), 'i32.const', maxResults1],
    ['i64', 'i64.const', [0]],
    ['i64 '.repeat(999), 'i64.const', maxResults1],
    ['f32', 'f32.const', [0]],
    ['f32 '.repeat(999), 'f32.const', maxResults1],
    ['f64', 'f64.const', [0]],
    ['f64 '.repeat(999), 'f64.const', maxResults1],
  ];
  for (let [types, constructor, params] of TESTS) {
    let {testCatch, testCatchRef} = wasmEvalText(`(module
      (tag $E (param ${types}))

      (func (export "testCatch") (result ${types})
        try_table (catch $E 0)
          ${params.map((x) => `${constructor} ${x}`).join(" ")}
          throw $E
        end
        unreachable
      )
      (func (export "testCatchRef") (result ${types})
        (block (result ${types} exnref)
          try_table (catch_ref $E 0)
            ${params.map((x) => `${constructor} ${x}`).join(" ")}
            throw $E
          end
          unreachable
        )
        drop
        return
      )

    )`).exports;
    assertEqResults(testCatch(), params);
    assertEqResults(testCatchRef(), params);
  }
}

// Test setting locals in conditional control flow
{
  let {test} = wasmEvalText(`(module
    (tag $E)

    (func (export "test") (param $shouldThrow i32) (result i32)
      (local $result i32)

      (block $join
        (block $catch
          try_table (catch $E $catch)
            local.get $shouldThrow
            if
              throw $E
            end
            (local.set $result i32.const 0)
            br $join
          end
        )
        (local.set $result i32.const 1)
        br $join
      )

      local.get $result
    )

  )`).exports;
  assertEq(test(0), 0);
  assertEq(test(1), 1);
}

// Matching catch clauses is done in order
{
  let {testCatch, testCatchRef, testCatchAll, testCatchAllRef} = wasmEvalText(`(module
    (tag $E)

    (func (export "testCatch")
      (block $good
        (block $bad
          try_table (catch $E $good) (catch $E $bad) (catch_all $bad)
            throw $E
          end
        )
        unreachable
      )
    )
    (func (export "testCatchAll")
      (block $good
        (block $bad
          try_table (catch_all $good) (catch $E $bad) (catch $E $bad)
            throw $E
          end
        )
        unreachable
      )
    )
    (func (export "testCatchRef")
      (block $good (result exnref)
        (block $bad (result exnref)
          try_table (catch_ref $E $good) (catch_ref $E $bad) (catch_all_ref $bad)
            throw $E
          end
          unreachable
        )
        unreachable
      )
      drop
    )
    (func (export "testCatchAllRef")
      (block $good (result exnref)
        (block $bad (result exnref)
          try_table (catch_all_ref $good) (catch_ref $E $bad) (catch_ref $E $bad)
            throw $E
          end
          unreachable
        )
        unreachable
      )
      drop
    )
  )`).exports;
  testCatch();
  testCatchAll();
  testCatchRef();
  testCatchAllRef();
}

// Test try_table as target of a delegate
{
  let {test} = wasmEvalText(`(module
    (tag $E)
    (func (export "test")
      block $good
        block $bad
          try_table $a (catch_all $good)
            try
              try
                throw $E
              delegate $a
            catch $E
              br $bad
            end
          end
        end
        unreachable
      end
    )
  )`).exports;
  test();
}

// Try table cannot be target of rethrow
{
  wasmFailValidateText(`(module
    (func
      try_table (catch_all 0) rethrow 0 end
    )
  )`, /rethrow target/);
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
    (func $innerRethrow (param externref)
      (block (result exnref)
        try_table (catch_ref $tag 0) (catch_all_ref 0)
          local.get 0
          call $throwJS
        end
        return
      )
      throw_ref
    )
    (func (export "test") (param externref)
      (block (result exnref)
        try_table (catch_ref $tag 0) (catch_all_ref 0)
          local.get 0
          call $innerRethrow
        end
        return
      )
      throw_ref
    )
  )`, {"": {tag, throwJS}}).exports;

  for (let value of values) {
    try {
      test(value);
      assertEq(true, false);
    } catch (thrownValue) {
      assertEq(thrownValue, value);
    }
  }
}

// WebAssembly.JSTag property is read-only and enumerable
WebAssembly.JSTag = null;
assertEq(WebAssembly.JSTag !== null, true);
assertEq(WebAssembly.propertyIsEnumerable('JSTag'), true);

// Test try_table catching JS exceptions and unpacking them using JSTag
{
  let tag = WebAssembly.JSTag;
  let values = [...WasmExternrefValues];
  function throwJS(value) {
    throw value;
  }
  let {test} = wasmEvalText(`(module
    (import "" "tag" (tag $tag (param externref)))
    (import "" "throwJS" (func $throwJS (param externref)))
    (func (export "test") (param externref) (result externref)
      try_table (catch $tag 0)
        local.get 0
        call $throwJS
      end
      unreachable
    )
  )`, {"": {tag, throwJS}}).exports;

  for (let value of values) {
    assertEq(value, test(value));
  }
}

// Test try_table catching JS exceptions using JSTag and unpacking them using JSTag
{
  let tag = WebAssembly.JSTag;
  let values = [...WasmExternrefValues];
  function throwJS(value) {
    throw new WebAssembly.Exception(tag, [value]);
  }
  let {test} = wasmEvalText(`(module
    (import "" "tag" (tag $tag (param externref)))
    (import "" "throwJS" (func $throwJS (param externref)))
    (func (export "test") (param externref) (result externref)
      try_table (catch $tag 0)
        local.get 0
        call $throwJS
      end
      unreachable
    )
  )`, {"": {tag, throwJS}}).exports;

  for (let value of values) {
    assertEq(value, test(value));
  }
}
