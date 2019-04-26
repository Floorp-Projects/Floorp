// |jit-test| skip-if: !wasmReftypesEnabled()

let ins
   = wasmEvalText(
      `(module
         (table 8 anyref)     ;; table 0
         (table $t 10 anyref) ;; table 1

         ;; fill/get for table 0, referenced implicitly
         (func (export "fill0") (param $i i32) (param $r anyref) (param $n i32)
           (table.fill (local.get $i) (local.get $r) (local.get $n))
         )
         (func (export "get0") (param $i i32) (result anyref)
           (table.get (local.get $i))
         )

         ;; fill/get for table 1, referenced explicitly
         (func (export "fill1") (param $i i32) (param $r anyref) (param $n i32)
           (table.fill $t (local.get $i) (local.get $r) (local.get $n))
         )
         (func (export "get1") (param $i i32) (result anyref)
           (table.get $t (local.get $i))
         )
       )`);

function Obj(n) {
    this.n = n;
}

function mkObj(n) {
    return new Obj(n);
}

const obj1 = mkObj(1);
const obj2 = mkObj(2);
const obj3 = mkObj(3);
const obj4 = mkObj(4);
const obj5 = mkObj(5);
const obj6 = mkObj(6);
const obj7 = mkObj(7);
const obj8 = mkObj(8);

// An initial test to ascertain that tables 0 and 1 are independent

// Fill in table 0, then check it.
assertEq(ins.exports.fill0(2, obj7, 5), undefined)
assertEq(ins.exports.fill0(1, obj8, 3), undefined);

function check_table0() {
    assertEq(ins.exports.get0(0), null);
    assertEq(ins.exports.get0(1), obj8);
    assertEq(ins.exports.get0(2), obj8);
    assertEq(ins.exports.get0(3), obj8);
    assertEq(ins.exports.get0(4), obj7);
    assertEq(ins.exports.get0(5), obj7);
    assertEq(ins.exports.get0(6), obj7);
    assertEq(ins.exports.get0(7), null);
}

// Check that table 0 has the expected content.
check_table0();

// Check that messing with table 0 above hasn't changed table 1.
for (let i = 0; i < 10; i++) {
    assertEq(ins.exports.get1(i), null);
}


// Now a bunch of tests involving only table 1.

// Within the table
assertEq(ins.exports.fill1(2, obj1, 3), undefined);
assertEq(ins.exports.get1(1), null);
assertEq(ins.exports.get1(2), obj1);
assertEq(ins.exports.get1(3), obj1);
assertEq(ins.exports.get1(4), obj1);
assertEq(ins.exports.get1(5), null);

// Within the table
assertEq(ins.exports.fill1(4, obj2, 2), undefined);
assertEq(ins.exports.get1(3), obj1);
assertEq(ins.exports.get1(4), obj2);
assertEq(ins.exports.get1(5), obj2);
assertEq(ins.exports.get1(6), null);

// Within the table
assertEq(ins.exports.fill1(4, obj3, 0), undefined);
assertEq(ins.exports.get1(3), obj1);
assertEq(ins.exports.get1(4), obj2);
assertEq(ins.exports.get1(5), obj2);

// Within the table
assertEq(ins.exports.fill1(8, obj4, 2), undefined);
assertEq(ins.exports.get1(7), null);
assertEq(ins.exports.get1(8), obj4);
assertEq(ins.exports.get1(9), obj4);

// Within the table
assertEq(ins.exports.fill1(9, null, 1), undefined);
assertEq(ins.exports.get1(8), obj4);
assertEq(ins.exports.get1(9), null);

// Within the table
assertEq(ins.exports.fill1(10, obj5, 0), undefined);
assertEq(ins.exports.get1(9), null);

// Partly outside the table
assertErrorMessage(() => ins.exports.fill1(8, obj6, 3),
                   RangeError, /table index out of bounds/);

assertEq(ins.exports.get1(7), null);
assertEq(ins.exports.get1(8), obj6);
assertEq(ins.exports.get1(9), obj6);


// Boundary tests on table 1: at the edge of the table.

// Length-zero fill1 at the edge of the table must succeed
assertEq(ins.exports.fill1(10, null, 0), undefined);

// Length-one fill1 at the edge of the table fails
assertErrorMessage(() => ins.exports.fill1(10, null, 1),
                   RangeError, /table index out of bounds/);

// Length-more-than-one fill1 at the edge of the table fails
assertErrorMessage(() => ins.exports.fill1(10, null, 2),
                   RangeError, /table index out of bounds/);


// Boundary tests on table 1: beyond the edge of the table:

// Length-zero fill1 beyond the edge of the table fails
assertErrorMessage(() => ins.exports.fill1(11, null, 0),
                   RangeError, /table index out of bounds/);

// Length-one fill1 beyond the edge of the table fails
assertErrorMessage(() => ins.exports.fill1(11, null, 1),
                   RangeError, /table index out of bounds/);

// Length-more-than-one fill1 beyond the edge of the table fails
assertErrorMessage(() => ins.exports.fill1(11, null, 2),
                   RangeError, /table index out of bounds/);


// Type errors.  Required sig is: (i32, anyref, i32) -> void

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $expected-3-args-got-0
        (table.fill $t)
     ))`),
     WebAssembly.CompileError, /popping value from empty stack/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $expected-3-args-got-1
        (table.fill $t (i32.const 0))
     ))`),
     WebAssembly.CompileError, /popping value from empty stack/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $expected-3-args-got-2
        (table.fill $t (ref.null) (i32.const 0))
     ))`),
     WebAssembly.CompileError, /popping value from empty stack/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $argty-1-wrong
        (table.fill $t (i32.const 0) (ref.null) (f64.const 0))
     ))`),
     WebAssembly.CompileError,
     /type mismatch: expression has type f64 but expected i32/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $argty-2-wrong
        (table.fill $t (i32.const 0) (f32.const 0) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /type mismatch: expression has type f32 but expected anyref/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $argty-3-wrong
        (table.fill $t (i64.const 0) (ref.null) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /type mismatch: expression has type i64 but expected i32/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 anyref)
      (func $retty-wrong (result i32)
        (table.fill $t (i32.const 0) (ref.null) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /popping value from empty stack/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 0 funcref)
      (func $tables-of-funcref-not-allowed (param $r anyref)
        (table.fill $t (i32.const 1) (local.get $r) (i32.const 1))
     ))`),
     WebAssembly.CompileError, /table.fill only on tables of anyref/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t1 1 anyref)
      (table $t2 1 funcref)
      (func $tables-of-funcref-not-allowed-2 (param $r anyref)
        (table.fill $t2 (i32.const 0) (local.get $r) (i32.const 1))
     ))`),
     WebAssembly.CompileError, /table.fill only on tables of anyref/);


// Following all the above tests on table 1, check table 0 hasn't changed.
check_table0();
