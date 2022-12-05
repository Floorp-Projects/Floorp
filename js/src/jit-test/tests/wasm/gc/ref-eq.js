// |jit-test| skip-if: !wasmGcEnabled()
//
// ref.eq is part of the gc feature, not the reftypes feature.

let { exports: { make, ref_eq, ref_eq_for_control } } = wasmEvalText(`(module
    (type $s (struct))

    (func (export "make") (result eqref) struct.new $s)

    (func (export "ref_eq") (param $a eqref) (param $b eqref) (result i32)
        (ref.eq (local.get $a) (local.get $b)))

    (func (export "ref_eq_for_control") (param $a eqref) (param $b eqref) (result f64)
        (if (result f64) (ref.eq (local.get $a) (local.get $b))
            (f64.const 5.0)
            (f64.const 3.0))))`);

let a = make();
let b = make();

assertEq(ref_eq(null, null), 1);
assertEq(ref_eq(null, a), 0);
assertEq(ref_eq(b, b), 1);
assertEq(ref_eq_for_control(null, null), 5);
assertEq(ref_eq_for_control(null, a), 3);
assertEq(ref_eq_for_control(b, b), 5);

