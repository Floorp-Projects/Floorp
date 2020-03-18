// This is fac-opt from fac.wast in the official testsuite, changed to use
// i32 instead of i64.
assertEq(wasmEvalText(`(module
  (func $fac-opt (param i32) (result i32)
    (local i32)
    (local.set 1 (i32.const 1))
    (block
      (br_if 0 (i32.lt_s (local.get 0) (i32.const 2)))
      (loop
        (local.set 1 (i32.mul (local.get 1) (local.get 0)))
        (local.set 0 (i32.add (local.get 0) (i32.const -1)))
        (br_if 0 (i32.gt_s (local.get 0) (i32.const 1)))
      )
    )
    (local.get 1)
  )

  (export "" 0)
)`).exports[""](10), 3628800);
