const N = 8;

function testTableFill(tbl_type, val_type, obj) {
  assertEq(obj.length, N);

  let ins
     = wasmEvalText(
        `(module
           (table 8 ${tbl_type})     ;; table 0
           (table $t 10 ${tbl_type}) ;; table 1

           ;; fill/get for table 0, referenced implicitly
           (func (export "fill0") (param $i i32) (param $r ${val_type}) (param $n i32)
             (table.fill (local.get $i) (local.get $r) (local.get $n))
           )
           (func (export "get0") (param $i i32) (result ${tbl_type})
             (table.get (local.get $i))
           )

           ;; fill/get for table 1, referenced explicitly
           (func (export "fill1") (param $i i32) (param $r ${val_type}) (param $n i32)
             (table.fill $t (local.get $i) (local.get $r) (local.get $n))
           )
           (func (export "get1") (param $i i32) (result ${tbl_type})
             (table.get $t (local.get $i))
           )
         )`);

  // An initial test to ascertain that tables 0 and 1 are independent

  // Fill in table 0, then check it.
  assertEq(ins.exports.fill0(2, obj[6], 5), undefined)
  assertEq(ins.exports.fill0(1, obj[7], 3), undefined);

  function check_table0() {
      assertEq(ins.exports.get0(0), null);
      assertEq(ins.exports.get0(1), obj[7]);
      assertEq(ins.exports.get0(2), obj[7]);
      assertEq(ins.exports.get0(3), obj[7]);
      assertEq(ins.exports.get0(4), obj[6]);
      assertEq(ins.exports.get0(5), obj[6]);
      assertEq(ins.exports.get0(6), obj[6]);
      assertEq(ins.exports.get0(7), null);
  }

  // Check that table 0 has the expected content.
  check_table0();

  // Check that messing with table 0 above hasn't changed table 1.
  for (let i = 0; i < 10; i++) {
      assertEq(ins.exports.get1(i), null);
  }

  // Now a bunch of tests involving only table 1.

  // Partly outside the table
  assertErrorMessage(() => ins.exports.fill1(8, obj[5], 3),
                     WebAssembly.RuntimeError, /index out of bounds/);

  assertEq(ins.exports.get1(7), null);
  assertEq(ins.exports.get1(8), null);
  assertEq(ins.exports.get1(9), null);

  // Within the table
  assertEq(ins.exports.fill1(2, obj[0], 3), undefined);
  assertEq(ins.exports.get1(1), null);
  assertEq(ins.exports.get1(2), obj[0]);
  assertEq(ins.exports.get1(3), obj[0]);
  assertEq(ins.exports.get1(4), obj[0]);
  assertEq(ins.exports.get1(5), null);

  // Within the table
  assertEq(ins.exports.fill1(4, obj[1], 2), undefined);
  assertEq(ins.exports.get1(3), obj[0]);
  assertEq(ins.exports.get1(4), obj[1]);
  assertEq(ins.exports.get1(5), obj[1]);
  assertEq(ins.exports.get1(6), null);

  // Within the table
  assertEq(ins.exports.fill1(4, obj[2], 0), undefined);
  assertEq(ins.exports.get1(3), obj[0]);
  assertEq(ins.exports.get1(4), obj[1]);
  assertEq(ins.exports.get1(5), obj[1]);

  // Within the table
  assertEq(ins.exports.fill1(8, obj[3], 2), undefined);
  assertEq(ins.exports.get1(7), null);
  assertEq(ins.exports.get1(8), obj[3]);
  assertEq(ins.exports.get1(9), obj[3]);

  // Within the table
  assertEq(ins.exports.fill1(9, null, 1), undefined);
  assertEq(ins.exports.get1(8), obj[3]);
  assertEq(ins.exports.get1(9), null);

  // Within the table
  assertEq(ins.exports.fill1(10, obj[4], 0), undefined);
  assertEq(ins.exports.get1(9), null);

  // Boundary tests on table 1: at the edge of the table.

  // Length-zero fill1 at the edge of the table must succeed
  assertEq(ins.exports.fill1(10, null, 0), undefined);

  // Length-one fill1 at the edge of the table fails
  assertErrorMessage(() => ins.exports.fill1(10, null, 1),
                     WebAssembly.RuntimeError, /index out of bounds/);

  // Length-more-than-one fill1 at the edge of the table fails
  assertErrorMessage(() => ins.exports.fill1(10, null, 2),
                     WebAssembly.RuntimeError, /index out of bounds/);


  // Boundary tests on table 1: beyond the edge of the table:

  // Length-zero fill1 beyond the edge of the table fails
  assertErrorMessage(() => ins.exports.fill1(11, null, 0),
                     WebAssembly.RuntimeError, /index out of bounds/);

  // Length-one fill1 beyond the edge of the table fails
  assertErrorMessage(() => ins.exports.fill1(11, null, 1),
                     WebAssembly.RuntimeError, /index out of bounds/);

  // Length-more-than-one fill1 beyond the edge of the table fails
  assertErrorMessage(() => ins.exports.fill1(11, null, 2),
                     WebAssembly.RuntimeError, /index out of bounds/);

  // Following all the above tests on table 1, check table 0 hasn't changed.
  check_table0();
}

var objs = [];
for (var i = 0; i < N; i++)
  objs[i] = {n:i};
testTableFill('externref', 'externref', objs);

var funcs = [];
for (var i = 0; i < N; i++)
  funcs[i] = wasmEvalText(`(module (func (export "x") (result i32) (i32.const ${i})))`).exports.x;
testTableFill('funcref', 'funcref', funcs);


// Type errors.  Required sig is: (i32, externref, i32) -> void

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $expected-3-args-got-0
        (table.fill $t)
     ))`),
     WebAssembly.CompileError, /(popping value from empty stack)|(nothing on stack)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $expected-3-args-got-1
        (table.fill $t (i32.const 0))
     ))`),
     WebAssembly.CompileError, /(popping value from empty stack)|(nothing on stack)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $expected-3-args-got-2
        (table.fill $t (ref.null extern) (i32.const 0))
     ))`),
     WebAssembly.CompileError, /(popping value from empty stack)|(nothing on stack)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $argty-1-wrong
        (table.fill $t (i32.const 0) (ref.null extern) (f64.const 0))
     ))`),
     WebAssembly.CompileError,
     /(type mismatch: expression has type f64 but expected i32)|(type mismatch: expected i32, found f64)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $argty-2-wrong
        (table.fill $t (i32.const 0) (f32.const 0) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /(type mismatch: expression has type f32 but expected externref)|(type mismatch: expected externref, found f32)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $argty-3-wrong
        (table.fill $t (i64.const 0) (ref.null extern) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /(type mismatch: expression has type i64 but expected i32)|(type mismatch: expected i32, found i64)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 10 externref)
      (func $retty-wrong (result i32)
        (table.fill $t (i32.const 0) (ref.null extern) (i32.const 0))
     ))`),
     WebAssembly.CompileError,
     /(popping value from empty stack)|(nothing on stack)/);

assertErrorMessage(() => wasmEvalText(
    `(module
       (table 8 funcref)
       (func (param $v externref)
         (table.fill (i32.const 0) (local.get $v) (i32.const 0)))
     )`),
     WebAssembly.CompileError,
     /(expression has type externref but expected funcref)|(type mismatch: expected funcref, found externref)/);
