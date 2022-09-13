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
assertEq(result[0], 2);
assertEq(result[1], new Float32Array([3.140000104904175])[0]);
result = structNewDefault();
assertEq(result[0], 0);
assertEq(result[1], 0);
result = structLarge();
assertEq(result[2], 3n);
assertEq(result[19], 19);

// array.new, array.new_default, and array.new_fixed

const { arrayNew, arrayNewDefault, arrayNewFixed } = wasmEvalText(`(module
    (type $r (struct (field i32) (field f32)))
    (type $a1 (array f64))
    (type $a2 (array i32))
    (type $a3 (array (ref null $r)))

    (global $g1 (ref null $a1) (array.new $a1 (f64.const 3.14) (i32.const 3)))
    (global $g2 (ref null $a2) (array.new_default $a2 (i32.const 2)))
    (global $g3 (ref null $a3) (array.new_fixed $a3 2
        (struct.new $r (i32.const 10) (f32.const 16.0))
        (ref.null $r)))

    (func (export "arrayNew") (result eqref) global.get $g1)
    (func (export "arrayNewDefault") (result eqref) global.get $g2)
    (func (export "arrayNewFixed") (result eqref) global.get $g3)
  )`).exports;

result = arrayNew();
assertEq(result.length, 3);
assertEq(result[0], 3.14);
assertEq(result[2], 3.14);
result = arrayNewDefault();
assertEq(result.length, 2);
assertEq(result[1], 0);
result = arrayNewFixed();
assertEq(result.length, 2);
assertEq(result[0][0], 10);
assertEq(result[0][1], 16);
assertEq(result[1], null);
