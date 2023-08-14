// Note that negative tests not having to do with table indices have been taken
// care of by tables-generalized.js.

///////////////////////////////////////////////////////////////////////////
//
// Positive tests

// - multiple local tables of misc type: syntax, validation, instantiation
// - element segments can point to a table
// - call-indirect can specify a table and will use it

var ins = wasmEvalText(
    `(module
      (table $t1 2 funcref)
      (table $t2 2 funcref)
      (type $ftype (func (param i32) (result i32)))
      (elem (table $t1) (i32.const 0) func $f1 $f2)
      (elem (table $t2) (i32.const 0) func $f3 $f4)
      (func $f1 (param $n i32) (result i32)
       (i32.add (local.get $n) (i32.const 1)))
      (func $f2 (param $n i32) (result i32)
       (i32.add (local.get $n) (i32.const 2)))
      (func $f3 (param $n i32) (result i32)
       (i32.add (local.get $n) (i32.const 3)))
      (func $f4 (param $n i32) (result i32)
       (i32.add (local.get $n) (i32.const 4)))
      (func (export "f") (param $fn i32) (param $n i32) (result i32)
       (call_indirect $t1 (type $ftype) (local.get $n) (local.get $fn)))
      (func (export "g") (param $fn i32) (param $n i32) (result i32)
       (call_indirect $t2 (type $ftype) (local.get $n) (local.get $fn))))`).exports;

assertEq(ins.f(0, 10), 11);
assertEq(ins.f(1, 10), 12);
assertEq(ins.g(0, 10), 13);
assertEq(ins.g(1, 10), 14);

// - export multiple tables.
//   note the first and third tables make the export list not start at zero,
//   and make it sparse

var ins = wasmEvalText(
    `(module
      (table $t0 (import "m" "t") 2 funcref)
      (table $t1 (export "t1") 2 funcref)
      (table 1 externref)
      (table $t2 (export "t2") 3 funcref))`,
    {m:{t: new WebAssembly.Table({element:"anyfunc", initial:2})}}).exports;

assertEq(ins.t1 instanceof WebAssembly.Table, true);
assertEq(ins.t1.length, 2);
assertEq(ins.t2 instanceof WebAssembly.Table, true);
assertEq(ins.t2.length, 3);

// - multiple imported tables of misc type
// - table.get and table.set can point to a table

var exp = {m:{t0: new WebAssembly.Table({element:"anyfunc", initial:2}),
              t1: new WebAssembly.Table({element:"externref", initial:3}),
              t2: new WebAssembly.Table({element:"anyfunc", initial:4}),
              t3: new WebAssembly.Table({element:"externref", initial:5})}};
var ins = wasmEvalText(
    `(module
      (table $t0 (import "m" "t0") 2 funcref)
      (table $t1 (import "m" "t1") 3 externref)
      (table $t2 (import "m" "t2") 4 funcref)
      (table $t3 (import "m" "t3") 5 externref)

      (type $id_i32_t (func (param i32) (result i32)))
      (func $id_i32 (param i32) (result i32) (local.get 0))
      (elem (table $t0) (i32.const 1) func $id_i32)

      (type $id_f64_t (func (param f64) (result f64)))
      (func $id_f64 (param f64) (result f64) (local.get 0))
      (elem (table $t2) (i32.const 3) func $id_f64)

      (func (export "f0") (param i32) (result i32)
       (call_indirect $t0 (type $id_i32_t) (local.get 0) (i32.const 1)))

      (func (export "f1") (param externref)
       (table.set $t1 (i32.const 2) (local.get 0)))

      (func (export "f2") (param f64) (result f64)
       (call_indirect $t2 (type $id_f64_t) (local.get 0) (i32.const 3)))

      (func (export "f3")
       (table.set $t3 (i32.const 4) (table.get $t1 (i32.const 2)))))`,

    exp).exports;

assertEq(ins.f0(37), 37);

var x = {}
ins.f1(x);
assertEq(exp.m.t1.get(2), x);

assertEq(ins.f2(3.25), 3.25);

ins.f3();
assertEq(exp.m.t3.get(4), x);

// - table.grow can point to a table
// - growing a table grows the right table but not the others
// - table.size on tables other than table 0

var exp = {m:{t0: new WebAssembly.Table({element:"externref", initial:2}),
              t1: new WebAssembly.Table({element:"externref", initial:3})}};
var ins = wasmEvalText(
    `(module
      (table $t0 (import "m" "t0") 2 externref)
      (table $t1 (import "m" "t1") 3 externref)
      (func (export "f") (result i32)
       (table.grow $t1 (ref.null extern) (i32.const 5)))
      (func (export "size0") (result i32)
       (table.size $t0))
      (func (export "size1") (result i32)
       (table.size $t1)))`,
    exp);

assertEq(ins.exports.f(), 3);
assertEq(exp.m.t1.length, 8);
assertEq(ins.exports.size0(), 2);
assertEq(ins.exports.size1(), 8);

// - table.copy can point to tables

var exp = {m:{t0: new WebAssembly.Table({element:"externref", initial:2}),
              t1: new WebAssembly.Table({element:"externref", initial:3})}};
var ins = wasmEvalText(
    `(module
      (table $t0 (import "m" "t0") 2 externref)
      (table $t1 (import "m" "t1") 3 externref)
      (func (export "f") (param $dest i32) (param $src i32) (param $len i32)
       (table.copy $t1 $t0 (local.get $dest) (local.get $src) (local.get $len))))`,
    exp);

exp.m.t0.set(0, {x:0});
exp.m.t0.set(1, {x:1});
ins.exports.f(1, 0, 2);
assertEq(exp.m.t1.get(1), exp.m.t0.get(0));
assertEq(exp.m.t1.get(2), exp.m.t0.get(1));

// - the table.copy syntax makes sense even in the non-parenthesized case

var ins = wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func (export "copy") (param $dest i32) (param $src i32) (param $len i32)
       local.get $dest
       local.get $src
       local.get $len
       table.copy $t1 $t0)
      (func (export "set") (param $n i32) (param $v externref)
       (table.set $t0 (local.get $n) (local.get $v)))
      (func (export "get") (param $n i32) (result externref)
       (table.get $t1 (local.get $n))))`,
    exp);

var values = [{x:0}, {x:1}];
ins.exports.set(0, values[0]);
ins.exports.set(1, values[1]);
ins.exports.copy(0, 0, 2);
assertEq(ins.exports.get(0), values[0]);
assertEq(ins.exports.get(1), values[1]);

// Copy beween an external table and a local table.  These cases are interesting
// mostly because the representations of the tables must be compatible; the copy
// in effect forces a representation for the function in the local table that
// captures the function's instance, at the latest at the time the copy is
// executed.
//
// In the case where the function is imported, it comes from a different module.
//
// Also tests:
// - local tables can be exported and re-imported in another module

var arg = 4;
for (let [a,b,x,y,result,init] of [['$t0', '$t1', '(export "t")', '', arg*13, true],
                               ['$t0', '$t1', '', '(export "t")', arg*13, true],
                               ['$t0', '$t1', '(import "m" "t")', '', arg*13, true],
                               ['$t1', '$t0', '(import "m" "t")', '', arg-11, false]])
{
    var otherins = wasmEvalText(
        `(module
          (table $t (export "t") 2 funcref)
          (type $fn1 (func (param i32) (result i32)))
          (func $f1 (param $n i32) (result i32)
           (i32.sub (local.get $n) (i32.const 11)))
          (elem (table $t) (i32.const 1) func $f1))`);

    let text =
        `(module
          (table ${a} ${x} 2 funcref)

          (table ${b} ${y} 2 funcref)
          (type $fn1 (func (param i32) (result i32)))
          (func $f1 (param $n i32) (result i32)
           (i32.mul (local.get $n) (i32.const 13)))
          ${init ? "(elem (table $t1) (i32.const 1) func $f1)" : ""}

          (func (export "f") (param $n i32) (result i32)
           (table.copy $t0 $t1 (i32.const 0) (i32.const 0) (i32.const 2))
           (call_indirect $t0 (type $fn1) (local.get $n) (i32.const 1))))`;
    var ins = wasmEvalText(text, {m: otherins.exports});

    assertEq(ins.exports.f(arg), result);
}

// - a table can be imported multiple times, and when it is, and one of them is grown,
//   they are all grown.
// - test the (import "m" "t" (table ...)) syntax
// - if table is grown from JS, wasm can observe the growth

var tbl = new WebAssembly.Table({element:"externref", initial:1});
var exp = {m: {t0: tbl, t1:tbl}};

var ins = wasmEvalText(
    `(module
      (import "m" "t0" (table $t0 1 externref))
      (import "m" "t1" (table $t1 1 externref))
      (table $t2 (export "t2") 1 funcref)
      (func (export "f") (result i32)
       (table.grow $t0 (ref.null extern) (i32.const 1)))
      (func (export "g") (result i32)
       (table.grow $t1 (ref.null extern) (i32.const 1)))
      (func (export "size") (result i32)
       (table.size $t2)))`,
    exp);

assertEq(ins.exports.f(), 1);
assertEq(ins.exports.g(), 2);
assertEq(ins.exports.f(), 3);
assertEq(ins.exports.g(), 4);
assertEq(tbl.length, 5);
ins.exports.t2.grow(3);
assertEq(ins.exports.size(), 4);

// - table.init on tables other than table 0

var ins = wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (table $t1 2 funcref)
      (elem func $f0 $f1) ;; 0
      (type $ftype (func (param i32) (result i32)))
      (func $f0 (param i32) (result i32)
       (i32.mul (local.get 0) (i32.const 13)))
      (func $f1 (param i32) (result i32)
       (i32.sub (local.get 0) (i32.const 11)))
      (func (export "call") (param i32) (param i32) (result i32)
       (call_indirect $t1 (type $ftype) (local.get 1) (local.get 0)))
      (func (export "init")
       (table.init $t1 0 (i32.const 0) (i32.const 0) (i32.const 2))))`);

ins.exports.init();
assertEq(ins.exports.call(0, 10), 130);
assertEq(ins.exports.call(1, 10), -1);

// - [white-box] if a multi-imported table of funcref is grown and the grown
//   part is properly initialized with functions then calls through both tables
//   in the grown area should succeed, ie, bounds checks should pass.  this is
//   an interesting case because we cache the table bounds for the benefit of
//   call_indirect, so here we're testing that the caches are updated properly
//   even when a table is observed multiple times (also by multiple modules).
//   there's some extra hair here because a table of funcref can be grown only
//   from JS at the moment.
// - also test that bounds checking continues to catch OOB calls

var tbl = new WebAssembly.Table({element:"anyfunc", initial:2});
var exp = {m:{t0: tbl, t1: tbl}};
var ins = wasmEvalText(
    `(module
      (import "m" "t0" (table $t0 2 funcref))
      (import "m" "t1" (table $t1 2 funcref))
      (type $ftype (func (param f64) (result f64)))
      (func (export "f") (param $n f64) (result f64)
       (f64.mul (local.get $n) (f64.const 3.25)))
      (func (export "do0") (param $i i32) (param $n f64) (result f64)
       (call_indirect $t0 (type $ftype) (local.get $n) (local.get $i)))
      (func (export "do1") (param $i i32) (param $n f64) (result f64)
       (call_indirect $t1 (type $ftype) (local.get $n) (local.get $i))))`,
    exp);
var ins2 = wasmEvalText(
    `(module
      (import "m" "t0" (table $t0 2 funcref))
      (import "m" "t1" (table $t1 2 funcref))
      (type $ftype (func (param f64) (result f64)))
      (func (export "do0") (param $i i32) (param $n f64) (result f64)
       (call_indirect $t0 (type $ftype) (local.get $n) (local.get $i)))
      (func (export "do1") (param $i i32) (param $n f64) (result f64)
       (call_indirect $t1 (type $ftype) (local.get $n) (local.get $i))))`,
    exp);

assertEq(tbl.grow(10), 2);
tbl.set(11, ins.exports.f);
assertEq(ins.exports.do0(11, 2.0), 6.5);
assertEq(ins.exports.do1(11, 4.0), 13.0);
assertEq(ins2.exports.do0(11, 2.0), 6.5);
assertEq(ins2.exports.do1(11, 4.0), 13.0);
assertErrorMessage(() => ins.exports.do0(12, 2.0),
                   WebAssembly.RuntimeError,
                   /index out of bounds/);
assertErrorMessage(() => ins2.exports.do0(12, 2.0),
                   WebAssembly.RuntimeError,
                   /index out of bounds/);
assertErrorMessage(() => ins.exports.do1(12, 2.0),
                   WebAssembly.RuntimeError,
                   /index out of bounds/);
assertErrorMessage(() => ins2.exports.do1(12, 2.0),
                   WebAssembly.RuntimeError,
                   /index out of bounds/);

///////////////////////////////////////////////////////////////////////////
//
// Negative tests

// Table index (statically) out of bounds

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (result externref)
       (table.get 2 (i32.const 0))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.get)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (param externref)
       (table.set 2 (i32.const 0) (local.get 0))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.set)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (param externref)
       (table.copy 0 2 (i32.const 0) (i32.const 0) (i32.const 2))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.copy)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (param externref)
       (table.copy 2 0 (i32.const 0) (i32.const 0) (i32.const 2))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.copy)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (result i32)
       (table.size 2)))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.size)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 externref)
      (table $t1 2 externref)
      (func $f (result i32)
       (table.grow 2 (ref.null extern) (i32.const 1))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.grow)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (elem func) ;; 0
      (func $f (result i32)
       (table.init 2 0 (i32.const 0) (i32.const 0) (i32.const 0))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for table.init)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (elem 2 (i32.const 0) func))`),
                   WebAssembly.CompileError,
                   /(table index out of range for element segment)|(table index out of bounds)/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (type $ft (func (param f64) (result i32)))
      (func $f (result i32)
       (call_indirect 2 (type $ft) (f64.const 3.14) (i32.const 0))))`),
                   WebAssembly.CompileError,
                   /(table index out of range for call_indirect)|(table index out of bounds)/);

// Syntax errors when parsing text

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (elem func) ;; 0
      (func $f (result i32)
       (table.init $t0 (i32.const 0) (i32.const 0) (i32.const 0))))`), // no segment
                   SyntaxError,
                   /failed to find name/);

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t0 2 funcref)
      (table $t1 2 funcref)
      (func $f
       (table.copy 0 (i32.const 0) (i32.const 0) (i32.const 2))))`), // target without source
                   SyntaxError,
                   /unexpected token, expected an index or an identifier/);

// Make sure that dead code doesn't prevent compilation.
wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func (param i32)
         (return)
         (table.get (local.get 0))
         (drop)
        )
    )`);

wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func (param i32) (param i32)
         (return)
         (table.grow (local.get 1))
         (drop)
        )
    )`);

wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func (param i32) (param externref)
         (return)
         (table.set (local.get 0) (local.get 1))
        )
    )`);

wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func
         (return)
         (table.size)
         (drop)
        )
    )`);

wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func
         (return)
         (table.copy (i32.const 0) (i32.const 1))
        )
    )`);
