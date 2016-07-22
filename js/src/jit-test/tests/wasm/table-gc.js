// |jit-test| --no-baseline
// Turn off baseline and since it messes up the GC finalization assertions by
// adding spurious edges to the GC graph.

load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');
const evalText = (str, imports) => new Instance(new Module(textToBinary(str)), imports);

var caller = `(type $v2i (func (result i32))) (func $call (param $i i32) (result i32) (call_indirect $v2i (get_local $i))) (export "call" $call)`
var callee = i => `(func $f${i} (type $v2i) (result i32) (i32.const ${i}))`;

// A table should not hold exported functions alive and exported functions
// should not hold their originating table alive. Live exported functions should
// hold instances alive. Nothing should hold the export object alive.
resetFinalizeCount();
var i = evalText(`(module (table (resizable 2)) (export "tbl" table) (elem 0 $f0) ${callee(0)} ${caller})`);
var e = i.exports;
var t = e.tbl;
var f = t.get(0);
assertEq(f(), e.call(0));
assertErrorMessage(() => e.call(1), Error, /bad wasm indirect call/);
assertErrorMessage(() => e.call(2), Error, /out-of-range/);
assertEq(finalizeCount(), 0);
i.edge = makeFinalizeObserver();
e.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
f = null;
gc();
assertEq(finalizeCount(), 1);
f = t.get(0);
f.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 1);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 2);
t = null;
gc();
assertEq(finalizeCount(), 3);
i = null;
gc();
assertEq(finalizeCount(), 3);
assertEq(f(), 0);
f = null;
gc();
assertEq(finalizeCount(), 5);

// A table should hold the instance of any of its elements alive.
resetFinalizeCount();
var i = evalText(`(module (table (resizable 1)) (export "tbl" table) (elem 0 $f0) ${callee(0)} ${caller})`);
var e = i.exports;
var t = e.tbl;
var f = t.get(0);
i.edge = makeFinalizeObserver();
e.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 1);
f = null;
gc();
assertEq(finalizeCount(), 2);
i = null;
gc();
assertEq(finalizeCount(), 2);
t = null;
gc();
assertEq(finalizeCount(), 4);

// The bad-indirect-call stub should (currently, could be changed later) keep
// the instance containing that stub alive.
resetFinalizeCount();
var i = evalText(`(module (table (resizable 2)) (export "tbl" table) ${caller})`);
var e = i.exports;
var t = e.tbl;
i.edge = makeFinalizeObserver();
e.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 1);
i = null;
gc();
assertEq(finalizeCount(), 1);
t = null;
gc();
assertEq(finalizeCount(), 3);

// Before initialization, a table is not bound to any instance.
resetFinalizeCount();
var i = evalText(`(module (func $f0 (result i32) (i32.const 0)) (export "f0" $f0))`);
var t = new Table({initial:4});
i.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i = null;
gc();
assertEq(finalizeCount(), 1);
t = null;
gc();
assertEq(finalizeCount(), 2);

// When a Table is created (uninitialized) and then first assigned, it keeps the
// first element's Instance alive (as above).
resetFinalizeCount();
var i = evalText(`(module (func $f (result i32) (i32.const 42)) (export "f" $f))`);
var f = i.exports.f;
var t = new Table({initial:1});
i.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
t.set(0, f);
assertEq(t.get(0), f);
assertEq(t.get(0)(), 42);
gc();
assertEq(finalizeCount(), 0);
f = null;
i.exports = null;
gc();
assertEq(finalizeCount(), 1);
assertEq(t.get(0)(), 42);
t.get(0).edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 2);
i = null;
gc();
assertEq(finalizeCount(), 2);
t.set(0, null);
assertEq(t.get(0), null);
gc();
assertEq(finalizeCount(), 2);
t = null;
gc();
assertEq(finalizeCount(), 4);
