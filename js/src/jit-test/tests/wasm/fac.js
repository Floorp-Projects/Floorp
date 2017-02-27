// This is fac-opt from fac.wast in the official testsuite, changed to use
// i32 instead of i64.
assertEq(wasmEvalText(`(module
  (func $fac-opt (param i32) (result i32)
    (local i32)
    (set_local 1 (i32.const 1))
    (block
      (br_if 0 (i32.lt_s (get_local 0) (i32.const 2)))
      (loop
        (set_local 1 (i32.mul (get_local 1) (get_local 0)))
        (set_local 0 (i32.add (get_local 0) (i32.const -1)))
        (br_if 0 (i32.gt_s (get_local 0) (i32.const 1)))
      )
    )
    (get_local 1)
  )

  (export "" 0)
)`).exports[""](10), 3628800);
