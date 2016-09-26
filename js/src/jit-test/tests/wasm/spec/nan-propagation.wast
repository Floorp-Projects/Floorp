;; Test that NaN values are propagated through arithmetic operators properly.

(module
  (func (export "add") (param $x f32) (param $y f32) (result f32) (f32.add (get_local $x) (get_local $y)))
  (func (export "sub") (param $x f32) (param $y f32) (result f32) (f32.sub (get_local $x) (get_local $y)))
  (func (export "mul") (param $x f32) (param $y f32) (result f32) (f32.mul (get_local $x) (get_local $y)))
  (func (export "div") (param $x f32) (param $y f32) (result f32) (f32.div (get_local $x) (get_local $y)))
  (func (export "sqrt") (param $x f32) (result f32) (f32.sqrt (get_local $x)))
  (func (export "min") (param $x f32) (param $y f32) (result f32) (f32.min (get_local $x) (get_local $y)))
  (func (export "max") (param $x f32) (param $y f32) (result f32) (f32.max (get_local $x) (get_local $y)))
  (func (export "ceil") (param $x f32) (result f32) (f32.ceil (get_local $x)))
  (func (export "floor") (param $x f32) (result f32) (f32.floor (get_local $x)))
  (func (export "trunc") (param $x f32) (result f32) (f32.trunc (get_local $x)))
  (func (export "nearest") (param $x f32) (result f32) (f32.nearest (get_local $x)))
  (func (export "abs") (param $x f32) (result f32) (f32.abs (get_local $x)))
  (func (export "neg") (param $x f32) (result f32) (f32.neg (get_local $x)))
  (func (export "copysign") (param $x f32) (param $y f32) (result f32) (f32.copysign (get_local $x) (get_local $y)))
)

(assert_return_nan (invoke "add" (f32.const infinity) (f32.const -infinity)))
(assert_return (invoke "add" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "add" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "add" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "add" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "add" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return_nan (invoke "sub" (f32.const infinity) (f32.const infinity)))
(assert_return (invoke "sub" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "sub" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "sub" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "sub" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "sub" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return_nan (invoke "mul" (f32.const infinity) (f32.const 0)))
(assert_return (invoke "mul" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "mul" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "mul" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "mul" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "mul" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return_nan (invoke "div" (f32.const 0) (f32.const 0)))
(assert_return (invoke "div" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "div" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "div" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "div" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "div" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return_nan (invoke "sqrt" (f32.const -0x1p0)))
(assert_return (invoke "sqrt" (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "sqrt" (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))

(assert_return (invoke "min" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "min" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "min" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "min" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "min" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return (invoke "max" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0x40f1e2))
(assert_return (invoke "max" (f32.const 0x1p0) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "max" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "max" (f32.const 0x1p0) (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))
(assert_return (invoke "max" (f32.const nan:0xf1e2) (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))

(assert_return (invoke "ceil" (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "ceil" (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))

(assert_return (invoke "floor" (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "floor" (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))

(assert_return (invoke "trunc" (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "trunc" (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))

(assert_return (invoke "nearest" (f32.const nan:0xf1e2)) (f32.const nan:0x40f1e2))
(assert_return (invoke "nearest" (f32.const -nan:0xf1e2)) (f32.const -nan:0x40f1e2))

(assert_return (invoke "abs" (f32.const nan:0xf1e2)) (f32.const nan:0xf1e2))
(assert_return (invoke "abs" (f32.const -nan:0xf1e2)) (f32.const nan:0xf1e2))

(assert_return (invoke "neg" (f32.const nan:0xf1e2)) (f32.const -nan:0xf1e2))
(assert_return (invoke "neg" (f32.const -nan:0xf1e2)) (f32.const nan:0xf1e2))

(assert_return (invoke "copysign" (f32.const nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0xf1e2))
(assert_return (invoke "copysign" (f32.const nan:0xf1e2) (f32.const -0x1p0)) (f32.const -nan:0xf1e2))
(assert_return (invoke "copysign" (f32.const -nan:0xf1e2) (f32.const 0x1p0)) (f32.const nan:0xf1e2))
(assert_return (invoke "copysign" (f32.const -nan:0xf1e2) (f32.const -0x1p0)) (f32.const -nan:0xf1e2))

(module
  (func (export "add") (param $x f64) (param $y f64) (result f64) (f64.add (get_local $x) (get_local $y)))
  (func (export "sub") (param $x f64) (param $y f64) (result f64) (f64.sub (get_local $x) (get_local $y)))
  (func (export "mul") (param $x f64) (param $y f64) (result f64) (f64.mul (get_local $x) (get_local $y)))
  (func (export "div") (param $x f64) (param $y f64) (result f64) (f64.div (get_local $x) (get_local $y)))
  (func (export "sqrt") (param $x f64) (result f64) (f64.sqrt (get_local $x)))
  (func (export "min") (param $x f64) (param $y f64) (result f64) (f64.min (get_local $x) (get_local $y)))
  (func (export "max") (param $x f64) (param $y f64) (result f64) (f64.max (get_local $x) (get_local $y)))
  (func (export "ceil") (param $x f64) (result f64) (f64.ceil (get_local $x)))
  (func (export "floor") (param $x f64) (result f64) (f64.floor (get_local $x)))
  (func (export "trunc") (param $x f64) (result f64) (f64.trunc (get_local $x)))
  (func (export "nearest") (param $x f64) (result f64) (f64.nearest (get_local $x)))
  (func (export "abs") (param $x f64) (result f64) (f64.abs (get_local $x)))
  (func (export "neg") (param $x f64) (result f64) (f64.neg (get_local $x)))
  (func (export "copysign") (param $x f64) (param $y f64) (result f64) (f64.copysign (get_local $x) (get_local $y)))
)

(assert_return_nan (invoke "add" (f64.const infinity) (f64.const -infinity)))
(assert_return (invoke "add" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "add" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "add" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "add" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "add" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return_nan (invoke "sub" (f64.const infinity) (f64.const infinity)))
(assert_return (invoke "sub" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "sub" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "sub" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "sub" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "sub" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return_nan (invoke "mul" (f64.const infinity) (f64.const 0)))
(assert_return (invoke "mul" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "mul" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "mul" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "mul" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "mul" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return_nan (invoke "div" (f64.const 0) (f64.const 0)))
(assert_return (invoke "div" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "div" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "div" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "div" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "div" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return_nan (invoke "sqrt" (f64.const -0x1p0)))
(assert_return (invoke "sqrt" (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "sqrt" (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))

(assert_return (invoke "min" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "min" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "min" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "min" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "min" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return (invoke "max" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "max" (f64.const 0x1p0) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "max" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "max" (f64.const 0x1p0) (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))
(assert_return (invoke "max" (f64.const nan:0xf1e2) (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))

(assert_return (invoke "ceil" (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "ceil" (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))

(assert_return (invoke "floor" (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "floor" (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))

(assert_return (invoke "trunc" (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "trunc" (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))

(assert_return (invoke "nearest" (f64.const nan:0xf1e2)) (f64.const nan:0x800000000f1e2))
(assert_return (invoke "nearest" (f64.const -nan:0xf1e2)) (f64.const -nan:0x800000000f1e2))

(assert_return (invoke "abs" (f64.const nan:0xf1e2)) (f64.const nan:0xf1e2))
(assert_return (invoke "abs" (f64.const -nan:0xf1e2)) (f64.const nan:0xf1e2))

(assert_return (invoke "neg" (f64.const nan:0xf1e2)) (f64.const -nan:0xf1e2))
(assert_return (invoke "neg" (f64.const -nan:0xf1e2)) (f64.const nan:0xf1e2))

(assert_return (invoke "copysign" (f64.const nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0xf1e2))
(assert_return (invoke "copysign" (f64.const nan:0xf1e2) (f64.const -0x1p0)) (f64.const -nan:0xf1e2))
(assert_return (invoke "copysign" (f64.const -nan:0xf1e2) (f64.const 0x1p0)) (f64.const nan:0xf1e2))
(assert_return (invoke "copysign" (f64.const -nan:0xf1e2) (f64.const -0x1p0)) (f64.const -nan:0xf1e2))

(module
  (func (export "f64.promote_f32") (param $x f32) (result f64) (f64.promote/f32 (get_local $x)))
  (func (export "f32.demote_f64") (param $x f64) (result f32) (f32.demote/f64 (get_local $x)))
  (func (export "f32.reinterpret_i32") (param $x i32) (result f32) (f32.reinterpret/i32 (get_local $x)))
  (func (export "f64.reinterpret_i64") (param $x i64) (result f64) (f64.reinterpret/i64 (get_local $x)))
)

(assert_return (invoke "f64.promote_f32" (f32.const nan:0xf1e2)) (f64.const nan:0x81e3c40000000))

(assert_return (invoke "f32.demote_f64" (f64.const nan:0xf1e2f1e2f1e2)) (f32.const nan:0x478f17))

(assert_return (invoke "f32.reinterpret_i32" (i32.const 0x7f876543)) (f32.const nan:0x76543))

(assert_return (invoke "f64.reinterpret_i64" (i64.const 0x7ff0123456789abc)) (f64.const nan:0x0123456789abc))
