(module
  (func $return_i32 (result i32)
    (unreachable))
  (func $return_f64 (result f64)
    (unreachable))

  (func $if (param i32) (result f32)
   (if (get_local 0) (unreachable) (f32.const 0)))

  (func $block
   (block (i32.const 1) (unreachable) (i32.const 2)))

  (func $return_i64 (result i64)
   (return (i64.const 1))
   (unreachable))

  (func $call (result f64)
   (call $return_i32)
   (unreachable))

  (func $misc1 (result i32)
    (i32.xor (unreachable) (i32.const 10))
  )

  (export "return_i32" $return_i32)
  (export "return_f64" $return_f64)
  (export "if" $if)
  (export "block" $block)
  (export "return_i64" $return_i64)
  (export "call" $call)
  (export "misc1" $misc1)
)

(assert_trap (invoke "return_i32") "unreachable executed")
(assert_trap (invoke "return_f64") "unreachable executed")
(assert_trap (invoke "if" (i32.const 1)) "unreachable executed")
(assert_return (invoke "if" (i32.const 0)) (f32.const 0))
(assert_trap (invoke "block") "unreachable executed")
(assert_return (invoke "return_i64") (i64.const 1))
(assert_trap (invoke "call") "unreachable executed")
(assert_trap (invoke "misc1") "unreachable executed")
