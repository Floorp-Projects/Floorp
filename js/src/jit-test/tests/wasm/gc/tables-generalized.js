// |jit-test| skip-if: !wasmGeneralizedTables()

///////////////////////////////////////////////////////////////////////////
//
// General table management in wasm

// Wasm: Create table-of-anyref

new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (table 10 anyref))`));

// Wasm: Import table-of-anyref
// JS: create table-of-anyref

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (table (import "m" "t") 10 anyref))`)),
                         {m:{t: new WebAssembly.Table({element:"anyref", initial:10})}});

new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (import "m" "t" (table 10 anyref)))`)),
                         {m:{t: new WebAssembly.Table({element:"anyref", initial:10})}});

// Wasm: Export table-of-anyref, initial values shall be null

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
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
       (gc_feature_opt_in 1)
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
       (gc_feature_opt_in 1)
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
       (gc_feature_opt_in 1)
       (func $f1 (result i32) (i32.const 0))
       (table 10 anyref)
       (elem (i32.const 0) $f1))`)),
                   WebAssembly.CompileError,
                   /only tables of 'anyfunc' may have element segments/);

// Wasm: table.init on table-of-anyref is forbidden

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (func $f1 (result i32) (i32.const 0))
       (table 10 anyref)
       (elem passive $f1)
       (func
         (table.init 0 (i32.const 0) (i32.const 0) (i32.const 0))))`)),
                   WebAssembly.CompileError,
                   /only tables of 'anyfunc' may have element segments/);

// Wasm: table types must match at link time

assertErrorMessage(
    () => new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (import "m" "t" (table 10 anyref)))`)),
                                   {m:{t: new WebAssembly.Table({element:"anyfunc", initial:10})}}),
    WebAssembly.LinkError,
    /imported table type mismatch/);

// call_indirect cannot reference table-of-anyref

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(
    `(module
       (gc_feature_opt_in 1)
       (table 10 anyref)
       (type $t (func (param i32) (result i32)))
       (func (result i32)
         (call_indirect $t (i32.const 37))))`)),
                   WebAssembly.CompileError,
                   /indirect calls must go through a table of 'anyfunc'/);

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

    // Temporary semantics is to convert to object and leave as object; once we
    // have a better wrapped anyref this will change, we won't be able to
    // observe the boxing.
    tbl.set(1, 42);
    let y = tbl.get(1);
    assertEq(typeof y, "object");
    assertEq(y instanceof Number, true);
    assertEq(y + 0, 42);
}

function dummy() { return 37 }
