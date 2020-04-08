// |jit-test| skip-if: !wasmReftypesEnabled()

///////////////////////////////////////////////////////////////////////////
//
// General table management in wasm

// Wasm: Create table-of-anyref

new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref))`));

// Wasm: Import table-of-anyref
// JS: create table-of-anyref

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (import "m" "t") 10 anyref))`)),
                         {m:{t: new WebAssembly.Table({element:"anyref", initial:10})}});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "m" "t" (table 10 anyref)))`)),
                         {m:{t: new WebAssembly.Table({element:"anyref", initial:10})}});

// Wasm: Export table-of-anyref, initial values shall be null

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 anyref))`)));
    let t = ins.exports.t;
    assertEq(t.length, 10);
    for (let i=0; i < t.length; i++)
        assertEq(t.get(0), null);
}

// JS: Exported table can be grown, and values are preserved

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 anyref))`)));
    let t = ins.exports.t;
    let objs = [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}];
    for (let i in objs)
        t.set(i, objs[i]);
    ins.exports.t.grow(10);
    assertEq(ins.exports.t.length, 20);
    for (let i in objs)
        assertEq(t.get(i), objs[i]);
}

// Wasm: table.copy between tables of anyref (currently source and destination
// are both table zero)

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 anyref)
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

// Wasm: table.copy from table(funcref) to table(anyref) should work

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table (export "t") 10 anyref)
       (func $f1)
       (func $f2)
       (func $f3)
       (func $f4)
       (func $f5)
       (table 5 funcref)
       (elem (table 1) (i32.const 0) func $f1 $f2 $f3 $f4 $f5)
       (func (export "f")
         (table.copy 0 1 (i32.const 5) (i32.const 0) (i32.const 5))))`)));
    ins.exports.f();
    let t = ins.exports.t;
    let xs = [];
    for (let i=0; i < 5; i++) {
        xs[i] = t.get(i+5);
        assertEq(typeof xs[i], "function");
    }
    for (let i=0; i < 5; i++) {
        for (j=i+1; j < 5; j++)
            assertEq(xs[i] != xs[j], true);
    }
}

// Wasm: table.copy from table(anyref) to table(funcref) should not work

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (table 10 anyref)
       (func (export "f")
         (table.copy 0 1 (i32.const 0) (i32.const 0) (i32.const 5))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected funcref/);

// Wasm: Element segments can target tables of anyref whether the element type
// is anyref or funcref.

for (let elem_ty of ["funcref", "anyref"]) {
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (export "f") (result i32) (i32.const 0))
       (func $f2 (result i32) (i32.const 0)) ;; on purpose not exported
       (table (export "t") 10 anyref)
       (elem (table 0) (i32.const 0) ${elem_ty} (ref.func $f1) (ref.func $f2))
       )`)));
    let t = ins.exports.t;
    let f = ins.exports.f;
    assertEq(t.get(0), f);
    assertEq(t.get(2), null);  // not much of a test since that's the default value
}

// Wasm: Element segments of anyref can't target tables of funcref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table (export "t") 10 funcref)
       (elem 0 (i32.const 0) anyref (ref.func $f1)))`)),
                   WebAssembly.CompileError,
                   /segment's element type must be subtype of table's element type/);

// Wasm: table.init on table-of-anyref is allowed whether the segment has
// anyrefs or funcrefs.

for (let elem_ty of ["funcref", "anyref"]) {
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table (export "t") 10 anyref)
       (elem ${elem_ty} (ref.func $f1))
       (func (export "f")
         (table.init 0 (i32.const 2) (i32.const 0) (i32.const 1))))`)));
    ins.exports.f();
    assertEq(typeof ins.exports.t.get(2), "function");
}

// Wasm: table.init on table-of-funcref is not allowed when the segment has
// anyref.

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (elem anyref (ref.null))
       (func
         (table.init 0 (i32.const 0) (i32.const 0) (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /expression has type anyref but expected funcref/);

// Wasm: table types must match at link time

assertErrorMessage(
    () => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (import "m" "t" (table 10 anyref)))`)),
                                   {m:{t: new WebAssembly.Table({element:"funcref", initial:10})}}),
    WebAssembly.LinkError,
    /imported table type mismatch/);

// call_indirect cannot reference table-of-anyref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (type $t (func (param i32) (result i32)))
       (func (result i32)
         (call_indirect (type $t) (i32.const 37))))`)),
                   WebAssembly.CompileError,
                   /indirect calls must go through a table of 'funcref'/);

///////////////////////////////////////////////////////////////////////////
//
// additional js api tests

{
    let tbl = new WebAssembly.Table({element:"anyref", initial:10});

    // Initial value.
    assertEq(tbl.get(0), null);

    // Identity preserving.
    let x = {hi: 48};
    tbl.set(0, x);
    assertEq(tbl.get(0), x);
    tbl.set(2, dummy);
    assertEq(tbl.get(2), dummy);
    tbl.set(2, null);
    assertEq(tbl.get(2), null);

    // Temporary semantics is to convert to object and leave as object; once we
    // have a better wrapped anyref this will change, we won't be able to
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
testTableGet('anyref', {});
testTableGet('funcref', wasmFun);

// table.get with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64) (result anyref)
         (table.get (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.get when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func (export "f") (param i32)
         (drop (table.get (local.get 0)))))`)),
                   WebAssembly.CompileError,
                   /table index out of range for table.get/);

// table.set in bounds with i32 x anyref - works, no value generated
// table.set with null - works
// table.set out of bounds - fails

function testTableSet(lhs_type, rhs_type, x) {
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 ${lhs_type})
           (func (export "set_ref") (param i32) (param ${rhs_type})
             (table.set (local.get 0) (local.get 1)))
           (func (export "set_null") (param i32)
             (table.set (local.get 0) (ref.null))))`);
    ins.exports.set_ref(3, x);
    assertEq(ins.exports.t.get(3), x);
    ins.exports.set_null(3);
    assertEq(ins.exports.t.get(3), null);

    assertErrorMessage(() => ins.exports.set_ref(10, x), WebAssembly.RuntimeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.set_ref(-1, x), WebAssembly.RuntimeError, /index out of bounds/);
}
testTableSet('anyref', 'anyref', {});
testTableSet('funcref', 'funcref', wasmFun);
testTableSet('anyref', 'funcref', wasmFun);

// Wasm: table.set on table(funcref) with anyref value should fail

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
        `(module
           (table (export "t") 10 funcref)
           (func (export "set_ref") (param i32) (param anyref)
             (table.set (local.get 0) (local.get 1))))`)),
                   WebAssembly.CompileError,
                   /type mismatch: expression has type anyref but expected funcref/);

// table.set with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64)
         (table.set (local.get 0) (ref.null))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.set with non-ref value - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
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
      (func (export "f") (param anyref)
       (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /table index out of range for table.set/);

function testTableGrow(lhs_type, rhs_type, x) {
  let ins = wasmEvalText(
      `(module
        (table (export "t") 10 20 ${lhs_type})
        (func (export "grow") (param i32) (result i32)
         (table.grow (ref.null) (local.get 0)))
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
testTableGrow('anyref', 'anyref', 42);
testTableGrow('funcref', 'funcref', wasmFun);
testTableGrow('anyref', 'funcref', wasmFun);

// Wasm: table.grow on table(funcref) with anyref initializer should fail

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
      `(module
        (table (export "t") 10 20 funcref)
        (func (export "grow2") (param i32) (param anyref) (result i32)
         (table.grow (local.get 1) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch: expression has type anyref but expected funcref/);

// Special case for private tables without a maximum

{
    let ins = wasmEvalText(
        `(module
          (table 10 anyref)
          (func (export "grow") (param i32) (result i32)
           (table.grow (ref.null) (local.get 0))))`);
    assertEq(ins.exports.grow(0), 10);
    assertEq(ins.exports.grow(1), 10);
    assertEq(ins.exports.grow(9), 11);
    assertEq(ins.exports.grow(0), 20);
}

// table.grow with non-i32 argument - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64)
        (table.grow (ref.null) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.grow when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func (export "f") (param i32)
        (table.grow (ref.null) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /table index out of range for table.grow/);

// table.size on table of anyref

for (let visibility of ['', '(export "t")', '(import "m" "t")']) {
    let exp = {m:{t: new WebAssembly.Table({element:"anyref",
                                            initial: 10,
                                            maximum: 20})}};
    let ins = wasmEvalText(
        `(module
          (table ${visibility} 10 20 anyref)
          (func (export "grow") (param i32) (result i32)
           (table.grow (ref.null) (local.get 0)))
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

// JS API for growing the table can take a fill argument, defaulting to null

let VALUES = [null,
              undefined,
              true,
              false,
              {x:1337},
              ["abracadabra"],
              1337,
              13.37,
              "hi",
              37n,
              Symbol("status"),
              () => 1337];

{
    let t = new WebAssembly.Table({element:"anyref", initial:0});
    t.grow(1);
    assertEq(t.get(t.length-1), null);
    let prev = null;
    for (let v of VALUES) {
        t.grow(2, v);
        assertEq(t.get(t.length-3), prev);
        assertEq(t.get(t.length-2), v);
        assertEq(t.get(t.length-1), v);
        prev = v;
    }
}

{
    let t = new WebAssembly.Table({element:"funcref", initial:0});
    let ins = wasmEvalText(
        `(module
           (func (export "f") (param i32) (result i32)
             (local.get 0)))`);
    t.grow(1);
    assertEq(t.get(t.length-1), null);
    t.grow(2, ins.exports.f);
    assertEq(t.get(t.length-3), null);
    assertEq(t.get(t.length-2), ins.exports.f);
    assertEq(t.get(t.length-1), ins.exports.f);
}

// If growing by zero elements there are no spurious writes

{
    let t = new WebAssembly.Table({element:"anyref", initial:1});
    t.set(0, 1337);
    t.grow(0, 1789);
    assertEq(t.get(0), 1337);
}
