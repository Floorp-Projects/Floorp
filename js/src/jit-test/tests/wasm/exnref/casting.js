// |jit-test| skip-if: !wasmGcEnabled()

// Test exnref
{
  const {
    refCast,
    refTest,
    branch,
    branchFail,
    refCastNullable,
    refTestNullable,
    branchNullable,
    branchFailNullable,
  } = wasmEvalText(`(module
    (tag $a)
    (func $make (param $null i32) (result exnref)
      (if (local.get $null)
        (then
          (return (ref.null exn))
        )
      )

      try_table (catch_all_ref 0)
        throw $a
      end
      unreachable
    )

    (func (export "refCast") (param $null i32)
      (call $make (local.get $null))
      ref.cast (ref exn)
      drop
    )
    (func (export "refTest") (param $null i32) (result i32)
      (call $make (local.get $null))
      ref.test (ref exn)
    )
    (func (export "branch") (param $null i32) (result i32)
      (block (result (ref exn))
        (call $make (local.get $null))
        br_on_cast 0 exnref (ref exn)
        drop
        (return (i32.const 0))
      )
      drop
      (return (i32.const 1))
    )
    (func (export "branchFail") (param $null i32) (result i32)
      (block (result exnref)
        (call $make (local.get $null))
        br_on_cast_fail 0 exnref (ref exn)
        drop
        (return (i32.const 1))
      )
      drop
      (return (i32.const 0))
    )

    (func (export "refCastNullable") (param $null i32)
      (call $make (local.get $null))
      ref.cast exnref
      drop
    )
    (func (export "refTestNullable") (param $null i32) (result i32)
      (call $make (local.get $null))
      ref.test exnref
    )
    (func (export "branchNullable") (param $null i32) (result i32)
      (block (result exnref)
        (call $make (local.get $null))
        br_on_cast 0 exnref exnref
        drop
        (return (i32.const 0))
      )
      drop
      (return (i32.const 1))
    )
    (func (export "branchFailNullable") (param $null i32) (result i32)
      (block (result exnref)
        (call $make (local.get $null))
        br_on_cast_fail 0 exnref exnref
        drop
        (return (i32.const 1))
      )
      drop
      (return (i32.const 0))
    )
  )`).exports;

  // cast non-null exnref -> (ref exn)
  refCast(0);
  assertEq(refTest(0), 1);
  assertEq(branch(0), 1);
  assertEq(branchFail(0), 1);

  // cast non-null exnref -> exnref
  refCastNullable(0);
  assertEq(refTestNullable(0), 1);
  assertEq(branchNullable(0), 1);
  assertEq(branchFailNullable(0), 1);

  // cast null exnref -> (ref exn)
  assertErrorMessage(() => refCast(1), WebAssembly.RuntimeError, /bad cast/);
  assertEq(refTest(1), 0);
  assertEq(branch(1), 0);
  assertEq(branchFail(1), 0);

  // cast null exnref -> exnref
  refCastNullable(1);
  assertEq(refTestNullable(1), 1);
  assertEq(branchNullable(1), 1);
  assertEq(branchFailNullable(1), 1);
}


// Test nullexnref
{
  const {
    refCastNull,
    refCastNonNull,
    refTestNull,
    refTestNonNull,
    branchNull,
    branchNonNull,
    branchFailNull,
    branchFailNonNull,
  } = wasmEvalText(`(module
    (func (export "refCastNull")
      ref.null noexn
      ref.cast nullexnref
      drop
    )
    (func (export "refCastNonNull")
      ref.null noexn
      ref.cast (ref noexn)
      drop
    )
    (func (export "refTestNull") (result i32)
      ref.null noexn
      ref.test nullexnref
    )
    (func (export "refTestNonNull") (result i32)
      ref.null noexn
      ref.test (ref noexn)
    )
    (func (export "branchNull") (result i32)
      (block (result nullexnref)
        ref.null noexn
        br_on_cast 0 exnref nullexnref
        drop
        (return (i32.const 0))
      )
      drop
      (return (i32.const 1))
    )
    (func (export "branchNonNull") (result i32)
      (block (result (ref noexn))
        ref.null noexn
        br_on_cast 0 exnref (ref noexn)
        drop
        (return (i32.const 0))
      )
      drop
      (return (i32.const 1))
    )
    (func (export "branchFailNull") (result i32)
      (block (result exnref)
        ref.null noexn
        br_on_cast_fail 0 exnref (ref noexn)
        drop
        (return (i32.const 1))
      )
      drop
      (return (i32.const 0))
    )
    (func (export "branchFailNonNull") (result i32)
      (block (result (ref exn))
        ref.null noexn
        br_on_cast_fail 0 exnref (ref null noexn)
        drop
        (return (i32.const 1))
      )
      drop
      (return (i32.const 0))
    )
  )`).exports;

    // null exceptions can be casted to nullexnref
    refCastNull();
    assertEq(refTestNull(), 1);
    assertEq(branchNull(), 1);
    assertEq(branchFailNull(), 0);

    // null exceptions cannot be casted to (ref noexn)
    assertErrorMessage(() => refCastNonNull(), WebAssembly.RuntimeError, /bad cast/);
    assertEq(refTestNonNull(), 0);
    assertEq(branchNonNull(), 0);
    assertEq(branchFailNonNull(), 1);
}
