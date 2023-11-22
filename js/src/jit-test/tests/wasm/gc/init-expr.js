// |jit-test| skip-if: !wasmGcEnabled()

// Test wasm::InitExpr GC specific constant expressions

// struct.new and struct.new_default

const { structNew, structNewDefault, structLarge } = wasmEvalText(`(module
    (type $r (struct (field i32) (field f32)))
    (type $xxl (struct
        (field i64) (field f64) (field i64) (field f64)
        (field i64) (field f64) (field i64) (field f64)
        (field i64) (field f64) (field i64) (field f64)
        (field i64) (field f64) (field i64) (field f64)
        (field i64) (field f64) (field i64) (field f64)))

    (global $g1 (ref null $r) (struct.new_default $r))

    (global $g2 (ref null $r) (struct.new $r
      (i32.const 2)
      (f32.const 3.14)))

    (global $gx (ref null $xxl) (struct.new $xxl
      (i64.const 1) (f64.const 2.) (i64.const 3) (f64.const 4.)
      (i64.const 5) (f64.const 2.) (i64.const 3) (f64.const 4.)
      (i64.const 1) (f64.const 8.) (i64.const 9) (f64.const 4.)
      (i64.const 1) (f64.const 2.) (i64.const 12) (f64.const 3.14)
      (i64.const 16) (f64.const 17.) (i64.const 18) (f64.const 19.)))
    
    (func (export "structNewDefault") (result eqref) global.get $g1)
    (func (export "structNew") (result eqref) global.get $g2)
    (func (export "structLarge") (result eqref) global.get $gx)
  )`).exports;

let result;
result = structNew();
assertEq(wasmGcReadField(result, 0), 2);
assertEq(wasmGcReadField(result, 1), new Float32Array([3.140000104904175])[0]);
result = structNewDefault();
assertEq(wasmGcReadField(result, 0), 0);
assertEq(wasmGcReadField(result, 1), 0);
result = structLarge();
assertEq(wasmGcReadField(result, 2), 3n);
assertEq(wasmGcReadField(result, 19), 19);

// array.new, array.new_default, array.new_fixed, and array.new_elem

const { arrayNew, arrayNewDefault, arrayNewFixed, arrayNewElem } = wasmEvalText(`(module
    (type $r (struct (field i32) (field f32)))
    (type $a1 (array f64))
    (type $a2 (array i32))
    (type $a3 (array (ref null $r)))

    (global $g1 (ref null $a1) (array.new $a1 (f64.const 3.14) (i32.const 3)))
    (global $g2 (ref null $a2) (array.new_default $a2 (i32.const 2)))
    (global $g3 (ref null $a3) (array.new_fixed $a3 2
        (struct.new $r (i32.const 10) (f32.const 16.0))
        (ref.null $r)))

    (elem $a3e (ref null $r)
      (item (struct.new $r (i32.const 1) (f32.const 2.0)))
      (item (struct.new $r (i32.const 3) (f32.const 4.0)))
      (item (struct.new $r (i32.const 5) (f32.const 6.0)))
    )

    (func (export "arrayNew") (result eqref) global.get $g1)
    (func (export "arrayNewDefault") (result eqref) global.get $g2)
    (func (export "arrayNewFixed") (result eqref) global.get $g3)
    (func (export "arrayNewElem") (result eqref)
      (array.new_elem $a3 $a3e (i32.const 0) (i32.const 3))
    )
  )`).exports;

result = arrayNew();
assertEq(wasmGcArrayLength(result), 3);
assertEq(wasmGcReadField(result, 0), 3.14);
assertEq(wasmGcReadField(result, 2), 3.14);

result = arrayNewDefault();
assertEq(wasmGcArrayLength(result), 2);
assertEq(wasmGcReadField(result, 1), 0);

result = arrayNewFixed();
assertEq(wasmGcArrayLength(result), 2);
assertEq(wasmGcReadField(wasmGcReadField(result, 0), 0), 10);
assertEq(wasmGcReadField(wasmGcReadField(result, 0), 1), 16);
assertEq(wasmGcReadField(result, 1), null);

result = arrayNewElem();
assertEq(wasmGcArrayLength(result), 3);
assertEq(wasmGcReadField(wasmGcReadField(result, 0), 0), 1);
assertEq(wasmGcReadField(wasmGcReadField(result, 0), 1), 2);
assertEq(wasmGcReadField(wasmGcReadField(result, 1), 0), 3);
assertEq(wasmGcReadField(wasmGcReadField(result, 1), 1), 4);
assertEq(wasmGcReadField(wasmGcReadField(result, 2), 0), 5);
assertEq(wasmGcReadField(wasmGcReadField(result, 2), 1), 6);

// any.convert_extern and extern.convert_any

let {testString, testArray} = wasmEvalText(`(module
        (type $array (array i32))
        (import "env" "s" (global $s (ref extern)))
        (global $s' (ref extern) (extern.convert_any (any.convert_extern (global.get $s))))
        (func (export "testString") (result (ref extern))
           (global.get $s'))
        (global $a (ref $array) (array.new_fixed $array 1 (i32.const 0)))
        (global $a' (ref any) (any.convert_extern (extern.convert_any (global.get $a))))
        (func (export "testArray") (result i32)
           (ref.eq (global.get $a) (ref.cast (ref eq) (global.get $a'))))
)`, {env:{s:"abc"}}).exports;

assertEq(testString(), 'abc');
assertEq(testArray(), 1);

wasmFailValidateText(`(module
	(global $g (ref extern) (extern.convert_any (any.convert_extern (ref.null extern))))
)`, /expected/);
wasmFailValidateText(`(module
	(global $g (ref extern) (any.convert_extern (extern.convert_any (ref.null any))))
)`, /expected/);

// Simple table initialization
{
  const { t1, t2, t1init } = wasmEvalText(`(module
    (type $s (struct (field i32)))
    (type $a1 (array f64))
    (type $a2 (array (ref null $s)))

    ;; passive segment
    (elem $e1 anyref
      (item (struct.new $s (i32.const 123)))
      (item (array.new $a1 (f64.const 234) (i32.const 3)))
      (item (array.new_default $a2 (i32.const 3)))
      (item (ref.i31 (i32.const 345)))
    )
    (table $t1 (export "t1") 4 4 anyref)

    ;; active segment
    (table $t2 (export "t2") anyref (elem
      (item (struct.new $s (i32.const 321)))
      (item (array.new $a1 (f64.const 432) (i32.const 3)))
      (item (array.new_default $a2 (i32.const 3)))
      (item (ref.i31 (i32.const 543)))
    ))

    (func (export "t1init") (table.init $t1 $e1 (i32.const 0) (i32.const 0) (i32.const 4)))
  )`).exports;

  assertEq(t1.get(0), null);
  assertEq(t1.get(1), null);
  assertEq(t1.get(2), null);
  assertEq(t1.get(3), null);

  assertEq(wasmGcReadField(t2.get(0), 0), 321);
  assertEq(wasmGcReadField(t2.get(1), 0), 432);
  assertEq(wasmGcReadField(t2.get(1), 1), 432);
  assertEq(wasmGcReadField(t2.get(1), 2), 432);
  assertEq(wasmGcReadField(t2.get(2), 0), null);
  assertEq(wasmGcReadField(t2.get(2), 1), null);
  assertEq(wasmGcReadField(t2.get(2), 2), null);
  assertEq(t2.get(3), 543);

  t1init();
  assertEq(wasmGcReadField(t1.get(0), 0), 123);
  assertEq(wasmGcReadField(t1.get(1), 0), 234);
  assertEq(wasmGcReadField(t1.get(1), 1), 234);
  assertEq(wasmGcReadField(t1.get(1), 2), 234);
  assertEq(wasmGcReadField(t1.get(2), 0), null);
  assertEq(wasmGcReadField(t1.get(2), 1), null);
  assertEq(wasmGcReadField(t1.get(2), 2), null);
  assertEq(t1.get(3), 345);
}

// The contents of passive segments are unique per instance and evaluated at
// instantiation time.
{
  const mod = new WebAssembly.Module(wasmTextToBinary(`(module
    (type $s (struct (field (mut i32))))
    (type $a (array (mut f64)))

    (elem $e anyref
      (item (struct.new $s (i32.const 123)))
      (item (array.new $a (f64.const 234) (i32.const 1)))
    )

    (table $t1 (export "t1") 2 2 anyref)
    (table $t2 (export "t2") 2 2 anyref)

    (start $init1)

    (func $init1
      (table.init $t1 $e (i32.const 0) (i32.const 0) (i32.const 2))
    )
    (func (export "init2")
      (table.init $t2 $e (i32.const 0) (i32.const 0) (i32.const 2))
    )
    (func (export "update1")
      (ref.cast (ref $s) (table.get $t1 (i32.const 0)))
      (struct.set $s 0 (i32.const 321))
      (ref.cast (ref $a) (table.get $t1 (i32.const 1)))
      (array.set $a (i32.const 0) (f64.const 432))
    )
    (func (export "update2")
      (ref.cast (ref $s) (table.get $t1 (i32.const 0)))
      (struct.set $s 0 (i32.const -321))
      (ref.cast (ref $a) (table.get $t1 (i32.const 1)))
      (array.set $a (i32.const 0) (f64.const -432))
    )
  )`));

  const { t1: t1_1, t2: t2_1, init2: init2_1, update1: update1_1, update2: update2_1 } = new WebAssembly.Instance(mod, {}).exports;
  const { t1: t1_2, t2: t2_2, init2: init2_2, update1: update1_2, update2: update2_2 } = new WebAssembly.Instance(mod, {}).exports;

  assertEq(wasmGcReadField(t1_1.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_1.get(1), 0), 234);
  assertEq(t2_1.get(0), null);
  assertEq(t2_1.get(1), null);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 234);
  assertEq(t2_2.get(0), null);
  assertEq(t2_2.get(1), null);

  update1_1();
  assertEq(wasmGcReadField(t1_1.get(0), 0), 321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), 432);
  assertEq(t2_1.get(0), null);
  assertEq(t2_1.get(1), null);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 234);
  assertEq(t2_2.get(0), null);
  assertEq(t2_2.get(1), null);

  init2_1();
  assertEq(wasmGcReadField(t1_1.get(0), 0), 321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), 432);
  assertEq(wasmGcReadField(t2_1.get(0), 0), 321);
  assertEq(wasmGcReadField(t2_1.get(1), 0), 432);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 234);
  assertEq(t2_2.get(0), null);
  assertEq(t2_2.get(1), null);

  init2_2();
  assertEq(wasmGcReadField(t1_1.get(0), 0), 321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), 432);
  assertEq(wasmGcReadField(t2_1.get(0), 0), 321);
  assertEq(wasmGcReadField(t2_1.get(1), 0), 432);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 234);
  assertEq(wasmGcReadField(t2_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t2_2.get(1), 0), 234);

  update2_1();
  assertEq(wasmGcReadField(t1_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t2_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t2_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 234);
  assertEq(wasmGcReadField(t2_2.get(0), 0), 123);
  assertEq(wasmGcReadField(t2_2.get(1), 0), 234);

  update1_2();
  assertEq(wasmGcReadField(t1_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t2_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t2_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t1_2.get(0), 0), 321);
  assertEq(wasmGcReadField(t1_2.get(1), 0), 432);
  assertEq(wasmGcReadField(t2_2.get(0), 0), 321);
  assertEq(wasmGcReadField(t2_2.get(1), 0), 432);

  update2_2();
  assertEq(wasmGcReadField(t1_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t1_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t2_1.get(0), 0), -321);
  assertEq(wasmGcReadField(t2_1.get(1), 0), -432);
  assertEq(wasmGcReadField(t1_2.get(0), 0), -321);
  assertEq(wasmGcReadField(t1_2.get(1), 0), -432);
  assertEq(wasmGcReadField(t2_2.get(0), 0), -321);
  assertEq(wasmGcReadField(t2_2.get(1), 0), -432);
}
