(module
  (func $empty
    (block)
    (block $l)
  )

  (func $singular (result i32)
    (block (i32.const 7))
  )

  (func $multi (result i32)
    (block (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8))
  )

  (func $effects (result i32)
    (local i32)
    (block
      (set_local 0 (i32.const 1))
      (set_local 0 (i32.mul (get_local 0) (i32.const 3)))
      (set_local 0 (i32.sub (get_local 0) (i32.const 5)))
      (set_local 0 (i32.mul (get_local 0) (i32.const 7)))
    )
    (get_local 0)
  )

  (export "empty" $empty)
  (export "singular" $singular)
  (export "multi" $multi)
  (export "effects" $effects)
)

(invoke "empty")
(assert_return (invoke "singular") (i32.const 7))
(assert_return (invoke "multi") (i32.const 8))
(assert_return (invoke "effects") (i32.const -14))
