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

// Wasm: element segments targeting table-of-anyref is forbidden

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table 10 anyref)
       (elem (i32.const 0) $f1))`)),
                   WebAssembly.CompileError,
                   /only tables of 'funcref' may have element segments/);

// Wasm: table.init on table-of-anyref is forbidden

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (func $f1 (result i32) (i32.const 0))
       (table 10 anyref)
       (elem passive $f1)
       (func
         (table.init 0 (i32.const 0) (i32.const 0) (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /only tables of 'funcref' may have element segments/);

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
         (call_indirect $t (i32.const 37))))`)),
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

// table.get in bounds - returns right value type & value
// table.get out of bounds - fails

{
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 anyref)
           (func (export "f") (param i32) (result anyref)
              (table.get (local.get 0))))`);
    let x = {};
    ins.exports.t.set(0, x);
    assertEq(ins.exports.f(0), x);
    assertEq(ins.exports.f(1), null);
    assertErrorMessage(() => ins.exports.f(10), RangeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.f(-5), RangeError, /index out of bounds/);
}

// table.get with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64) (result anyref)
         (table.get (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.get on table of funcref - fails validation because funcref is not expressible
// Both with and without anyref support

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (func (export "f") (param i32)
         (drop (table.get (local.get 0)))))`)),
                   WebAssembly.CompileError,
                   /table.get only on tables of anyref/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 funcref)
       (func (export "f") (param i32)
         (drop (table.get (local.get 0)))))`)),
                   WebAssembly.CompileError,
                   /table.get only on tables of anyref/);

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

{
    let ins = wasmEvalText(
        `(module
           (table (export "t") 10 anyref)
           (func (export "set_anyref") (param i32) (param anyref)
             (table.set (local.get 0) (local.get 1)))
           (func (export "set_null") (param i32)
             (table.set (local.get 0) (ref.null))))`);
    let x = {};
    ins.exports.set_anyref(3, x);
    assertEq(ins.exports.t.get(3), x);
    ins.exports.set_null(3);
    assertEq(ins.exports.t.get(3), null);

    assertErrorMessage(() => ins.exports.set_anyref(10, x), RangeError, /index out of bounds/);
    assertErrorMessage(() => ins.exports.set_anyref(-1, x), RangeError, /index out of bounds/);
}

// table.set with non-i32 index - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64)
         (table.set (local.get 0) (ref.null))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.set with non-anyref value - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (table 10 anyref)
       (func (export "f") (param f64)
         (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /type mismatch/);

// table.set on table of funcref - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (table 10 funcref)
      (func (export "f") (param anyref)
       (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /table.set only on tables of anyref/);

// table.set when there are no tables - fails validation

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
      (func (export "f") (param anyref)
       (table.set (i32.const 0) (local.get 0))))`)),
                   WebAssembly.CompileError,
                   /table index out of range for table.set/);

// we can grow table of anyref
// table.grow with zero delta - always works even at maximum
// table.grow with delta - works and returns correct old value
// table.grow with delta at upper limit - fails
// table.grow with negative delta - fails

let ins = wasmEvalText(
    `(module
      (table (export "t") 10 20 anyref)
      (func (export "grow") (param i32) (result i32)
       (table.grow (ref.null) (local.get 0))))`);
assertEq(ins.exports.grow(0), 10);
assertEq(ins.exports.t.length, 10);
assertEq(ins.exports.grow(1), 10);
assertEq(ins.exports.t.length, 11);
assertEq(ins.exports.t.get(10), null);
assertEq(ins.exports.grow(9), 11);
assertEq(ins.exports.t.length, 20);
assertEq(ins.exports.t.get(19), null);
assertEq(ins.exports.grow(0), 20);

// The JS API throws if it can't grow
assertErrorMessage(() => ins.exports.t.grow(1), RangeError, /failed to grow table/);
assertErrorMessage(() => ins.exports.t.grow(-1), TypeError, /bad [Tt]able grow delta/);

// The wasm API does not throw if it can't grow, but returns -1
assertEq(ins.exports.grow(1), -1);
assertEq(ins.exports.t.length, 20);
assertEq(ins.exports.grow(-1), -1);
assertEq(ins.exports.t.length, 20)

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

// Can't grow table of funcref yet

assertErrorMessage(() => wasmEvalText(
    `(module
      (table $t 2 funcref)
      (func $f
       (drop (table.grow (ref.null) (i32.const 1)))))`),
                   WebAssembly.CompileError,
                   /table.grow only on tables of anyref/);

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
