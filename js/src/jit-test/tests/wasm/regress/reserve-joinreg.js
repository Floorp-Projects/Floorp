// Bug 1280933, excerpted from binary test case provided there.

wasmEvalText(
`(module
  (func $func0 (param $arg0 i32) (result i32) (local $var0 i64)
  (local.set $var0 (i64.extend_i32_u (local.get $arg0)))
  (i32.wrap_i64
   (i64.add
    (block (result i64)
      (block $label1
        (loop $label0
          (drop
            (block $label2 (result i64)
              (br_table $label2 (i64.const 0) (local.get $arg0))
            )
          )
        (local.set $var0 (i64.mul (i64.const 2) (local.get $var0)))
      )
    )
    (local.tee $var0 (i64.add (i64.const 4) (local.get $var0))))
    (i64.const 1))))
  (export "" (func 0)))`);
