// |jit-test| skip-if: !wasmReftypesEnabled()

///////////////////////////////////////////////////////////////////////////
//
// General table management in wasm

// Wasm: Create table-of-externref

new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref))`));

// Wasm: Import table-of-externref
// JS: create table-of-externref

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (import "m" "t") 10 externref))`)),
                         {m:{t: new WebAssembly.Table({element:"externref", initial:10})}});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "m" "t" (table 10 externref)))`)),
                         {m:{t: new WebAssembly.Table({element:"externref", initial:10})}});

// Wasm: Export table-of-externref, initial values shall be null

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 externref))`)));
    let t = ins.exports.t;
    assertEq(t.length, 10);
    for (let i=0; i < t.length; i++)
        assertEq(t.get(0), null);
}

// JS: Exported table can be grown, and values are preserved

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 externref))`)));
    let t = ins.exports.t;
    let objs = [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}];
    for (let i in objs)
        t.set(i, objs[i]);
    ins.exports.t.grow(10);
    assertEq(ins.exports.t.length, 20);
    for (let i in objs)
        assertEq(t.get(i), objs[i]);
}

// Wasm: table.copy between tables of externref (currently source and destination
// are both table zero)

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 externref)
       (func (export "f")
         (table.copy (i32.const 5) (i32.const 0) (i32.const 3))))`)));
    let t = ins.exports.t;
    let objs = [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}];
    for (let i in objs)
        t.set(i, objs[i]);
    ins.exports.f();
    assertEq(t.get(0), objs[0]);
    assertEq(t.get(1), objs[1]);
    assertEq(t.get(2), objs[2]);
    assertEq(t.get(3), objs[3]);
    assertEq(t.get(4), objs[4]);
    assertEq(t.get(5), objs[0]);
    assertEq(t.get(6), objs[1]);
    assertEq(t.get(7), objs[2]);
    assertEq(t.get(8), objs[8]);
    assertEq(t.get(9), objs[9]);
}

// Wasm: table.copy from table(funcref) to table(externref) should not work

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
  `(module
     (table (export "t") 10 externref)
     (func $f1)
     (func $f2)
     (func $f3)
     (func $f4)
     (func $f5)
     (table 5 funcref)
     (elem (table 1) (i32.const 0) func $f1 $f2 $f3 $f4 $f5)
     (func (export "f")
       (table.copy 0 1 (i32.const 5) (i32.const 0) (i32.const 5))))`)),
    WebAssembly.CompileError,
    /(expression has type funcref but expected externref)|(type mismatch)/);

// Wasm: table.copy from table(externref) to table(funcref) should not work

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (table 10 externref)
       (func (export "f")
         (table.copy 0 1 (i32.const 0) (i32.const 0) (i32.const 5))))`)),
                   WebAssembly.CompileError,
                   /(expression has type externref but expected funcref)|(type mismatch)/);

// Wasm: Element segments of funcref can't target tables of externref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table (export "t") 10 externref)
       (elem 0 (i32.const 0) funcref (ref.func $f1)))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);


// Wasm: Element segments of externref can't target tables of funcref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 funcref)
       (elem 0 (i32.const 0) externref (ref.null extern)))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// Wasm: table.init on table-of-externref is not allowed when the segment has
// funcref.

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table 10 externref)
       (elem funcref (ref.func $f1))
       (func
         (table.init 0 (i32.const 0) (i32.const 0) (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /(expression has type funcref but expected externref)|(type mismatch)/);


// Wasm: table.init on table-of-funcref is not allowed when the segment has
// externref.

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (elem externref (ref.null extern))
       (func
         (table.init 0 (i32.const 0) (i32.const 0) (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /(expression has type externref but expected funcref)|(type mismatch)/);

// Wasm: table types must match at link time

assertErrorMessage(
    () => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "m" "t" (table 10 externref)))`)),
                                   {m:{t: new WebAssembly.Table({element:"funcref", initial:10})}}),
    WebAssembly.LinkError,
    /imported table type mismatch/);

// call_indirect cannot reference table-of-externref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref)
       (type $t (func (param i32) (result i32)))
       (func (result i32)
         (call_indirect (type $t) (i32.const 37))))`)),
                   WebAssembly.CompileError,
                   /(indirect calls must go through a table of 'funcref')|(indirect calls must go through a table of funcref)/);

///////////////////////////////////////////////////////////////////////////
//
// additional js api tests

{
    let tbl = new WebAssembly.Table({element:"externref", initial:10});

    // Initial value is undefined. This is different from when tables are
    // created inside wasm.
    assertEq(tbl.get(0), undefined);

    // Identity preserving.
    let x = {hi: 48};
    tbl.set(0, x);
    assertEq(tbl.get(0), x);
    tbl.set(2, dummy);
    assertEq(tbl.get(2), dummy);
    tbl.set(2, null);
    assertEq(tbl.get(2), null);

    // Temporary semantics is to convert to object and leave as object; once we
    // have a better wrapped externref this will change, we won't be able to
    // observe the boxing.
    tbl.set(1, 42);
    let y = tbl.get(1);
    assertEq(typeof y, "number");
    assertEq(y, 42);
}

function dummy() { return 37 }

///////////////////////////////////////////////////////////////////////////
//
// table.get and table.set

const wasmFun = wasmEvalText(`(module (func (export "x")))`).exports.x;

// table.get in bounds - returns right value type & value
// table.get out of bounds - fails

function testTableGet(type, x) {
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 ${type})
           (func (export "f") (param i32) (result ${type})
              (table.get (local.get 0))))`);
    ins.exports.t.set(0, x);
    assertEq(ins.exports.f(0), x);
    assertEq(ins.exports.f(1), null);
    assertErrorMessage(() => ins.exports.f(10), WebAssembly.RuntimeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.f(-5), WebAssembly.RuntimeError, /index out of bounds/);
}
testTableGet('externref', {});
testTableGet('funcref', wasmFun);

// table.get with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref)
       (func (export "f") (param f64) (result externref)
         (table.get (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.get when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func (export "f") (param i32)
         (drop (table.get (local.get 0)))))`)),
                   WebAssembly.CompileError,
                   /(table index out of range for table.get)|(table index out of bounds)/);

// table.set in bounds with i32 x externref - works, no value generated
// table.set with null - works
// table.set out of bounds - fails

function testTableSet(lhs_type, rhs_type, rhs_reftype, x) {
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 ${lhs_type})
           (func (export "set_ref") (param i32) (param ${rhs_type})
             (table.set (local.get 0) (local.get 1)))
           (func (export "set_null") (param i32)
             (table.set (local.get 0) (ref.null ${rhs_reftype}))))`);
    ins.exports.set_ref(3, x);
    assertEq(ins.exports.t.get(3), x);
    ins.exports.set_null(3);
    assertEq(ins.exports.t.get(3), null);

    assertErrorMessage(() => ins.exports.set_ref(10, x), WebAssembly.RuntimeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.set_ref(-1, x), WebAssembly.RuntimeError, /index out of bounds/);
}
testTableSet('externref', 'externref', 'extern', {});
testTableSet('funcref', 'funcref', 'func', wasmFun);

// Wasm: table.set on table(funcref) with externref value should fail

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
        `(module
           (table (export "t") 10 funcref)
           (func (export "set_ref") (param i32) (param externref)
             (table.set (local.get 0) (local.get 1))))`)),
                   WebAssembly.CompileError,
                   /(type mismatch: expression has type externref but expected funcref)|(type mismatch: expected funcref, found externref)/);

// table.set with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref)
       (func (export "f") (param f64)
         (table.set (local.get 0) (ref.null extern))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.set with non-ref value - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref)
       (func (export "f") (param f64)
         (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);
assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (func (export "f") (param f64)
         (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.set when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (export "f") (param externref)
       (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /(table index out of range for table.set)|(table index out of bounds)/);

function testTableGrow(lhs_type, lhs_reftype, rhs_type, x) {
  let ins = wasmEvalText(
      `(module
        (table (export "t") 10 20 ${lhs_type})
        (func (export "grow") (param i32) (result i32)
         (table.grow (ref.null ${lhs_reftype}) (local.get 0)))
        (func (export "grow2") (param i32) (param ${rhs_type}) (result i32)
         (table.grow (local.get 1) (local.get 0))))`);

  // we can grow table of references
  // table.grow with zero delta - always works even at maximum
  // table.grow with delta - works and returns correct old value
  // table.grow with delta at upper limit - fails
  // table.grow with negative delta - fails
  assertEq(ins.exports.grow(0), 10);
  assertEq(ins.exports.t.length, 10);
  assertEq(ins.exports.grow(1), 10);
  assertEq(ins.exports.t.length, 11);
  assertEq(ins.exports.t.get(10), null);
  assertEq(ins.exports.grow2(9, x), 11);
  assertEq(ins.exports.t.length, 20);
  for (var i = 11; i < 20; i++)
    assertEq(ins.exports.t.get(i), x);
  assertEq(ins.exports.grow(0), 20);

  // The JS API throws if it can't grow
  assertErrorMessage(() => ins.exports.t.grow(1), RangeError, /failed to grow table/);
  assertErrorMessage(() => ins.exports.t.grow(-1), TypeError, /bad [Tt]able grow delta/);

  // The wasm API does not throw if it can't grow, but returns -1
  assertEq(ins.exports.grow(1), -1);
  assertEq(ins.exports.t.length, 20);
  assertEq(ins.exports.grow(-1), -1);
  assertEq(ins.exports.t.length, 20)
}
testTableGrow('externref', 'extern', 'externref', 42);
testTableGrow('funcref', 'func', 'funcref', wasmFun);

// Wasm: table.grow on table(funcref) with externref initializer should fail

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
      `(module
        (table (export "t") 10 20 funcref)
        (func (export "grow2") (param i32) (param externref) (result i32)
         (table.grow (local.get 1) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /(type mismatch: expression has type externref but expected funcref)|(type mismatch: expected funcref, found externref)/);

// Special case for private tables without a maximum

{
    let ins = wasmEvalText(
        `(module
          (table 10 externref)
          (func (export "grow") (param i32) (result i32)
           (table.grow (ref.null extern) (local.get 0))))`);
    assertEq(ins.exports.grow(0), 10);
    assertEq(ins.exports.grow(1), 10);
    assertEq(ins.exports.grow(9), 11);
    assertEq(ins.exports.grow(0), 20);
}

// table.grow with non-i32 argument - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 externref)
       (func (export "f") (param f64)
        (table.grow (ref.null extern) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.grow when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func (export "f") (param i32)
        (table.grow (ref.null extern) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /(table index out of range for table.grow)|(table index out of bounds)/);

// table.size on table of externref

for (let visibility of ['', '(export "t")', '(import "m" "t")']) {
    let exp = {m:{t: new WebAssembly.Table({element:"externref",
                                            initial: 10,
                                            maximum: 20})}};
    let ins = wasmEvalText(
        `(module
          (table ${visibility} 10 20 externref)
          (func (export "grow") (param i32) (result i32)
           (table.grow (ref.null extern) (local.get 0)))
          (func (export "size") (result i32)
           (table.size)))`,
        exp);
    assertEq(ins.exports.grow(0), 10);
    assertEq(ins.exports.size(), 10);
    assertEq(ins.exports.grow(1), 10);
    assertEq(ins.exports.size(), 11);
    assertEq(ins.exports.grow(9), 11);
    assertEq(ins.exports.size(), 20);
    assertEq(ins.exports.grow(0), 20);
    assertEq(ins.exports.size(), 20);
}

// table.size on table of funcref

{
    let ins = wasmEvalText(
        `(module
          (table (export "t") 2 funcref)
          (func (export "f") (result i32)
           (table.size)))`);
    assertEq(ins.exports.f(), 2);
    ins.exports.t.grow(1);
    assertEq(ins.exports.f(), 3);
}
