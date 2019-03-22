// |jit-test| --no-baseline
// Turn off baseline and since it messes up the GC finalization assertions by
// adding spurious edges to the GC graph.

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;
const RuntimeError = WebAssembly.RuntimeError;

var caller = `(type $v2i (func (result i32))) (func $call (param $i i32) (result i32) (call_indirect $v2i (local.get $i))) (export "call" $call)`
var callee = i => `(func $f${i} (type $v2i) (i32.const ${i}))`;

// A table should not hold exported functions alive and exported functions
// should not hold their originating table alive. Live exported functions should
// hold instances alive and instances hold imported tables alive. Nothing
// should hold the export object alive.
resetFinalizeCount();
var i = wasmEvalText(`(module (table 2 funcref) (export "tbl" table) (elem (i32.const 0) $f0) ${callee(0)} ${caller})`);
var e = i.exports;
var t = e.tbl;
var f = t.get(0);
assertEq(f(), e.call(0));
assertErrorMessage(() => e.call(1), RuntimeError, /indirect call to null/);
assertErrorMessage(() => e.call(2), RuntimeError, /index out of bounds/);
assertEq(finalizeCount(), 0);
i.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
f.x = 42;
f = null;
gc();
assertEq(finalizeCount(), 0);
f = t.get(0);
assertEq(f.x, 42);
gc();
assertEq(finalizeCount(), 0);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 0);
t = null;
gc();
assertEq(finalizeCount(), 0);
i = null;
gc();
assertEq(finalizeCount(), 0);
assertEq(f(), 0);
f = null;
gc();
assertEq(finalizeCount(), 3);

// A table should hold the instance of any of its elements alive.
resetFinalizeCount();
var i = wasmEvalText(`(module (table 1 funcref) (export "tbl" table) (elem (i32.const 0) $f0) ${callee(0)} ${caller})`);
var e = i.exports;
var t = e.tbl;
var f = t.get(0);
i.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 0);
f = null;
gc();
assertEq(finalizeCount(), 0);
i = null;
gc();
assertEq(finalizeCount(), 0);
t = null;
gc();
assertEq(finalizeCount(), 3);

// Null elements shouldn't keep anything alive.
resetFinalizeCount();
var i = wasmEvalText(`(module (table 2 funcref) (export "tbl" table) ${caller})`);
var e = i.exports;
var t = e.tbl;
i.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i.exports = null;
e = null;
gc();
assertEq(finalizeCount(), 0);
i = null;
gc();
assertEq(finalizeCount(), 1);
t = null;
gc();
assertEq(finalizeCount(), 2);

// Before initialization, a table is not bound to any instance.
resetFinalizeCount();
var i = wasmEvalText(`(module (func $f0 (result i32) (i32.const 0)) (export "f0" $f0))`);
var t = new Table({initial:4, element:"funcref"});
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
var i = wasmEvalText(`(module (func $f (result i32) (i32.const 42)) (export "f" $f))`);
var f = i.exports.f;
var t = new Table({initial:1, element:"funcref"});
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
assertEq(finalizeCount(), 0);
assertEq(t.get(0)(), 42);
i = null;
gc();
assertEq(finalizeCount(), 0);
t.set(0, null);
assertEq(t.get(0), null);
gc();
assertEq(finalizeCount(), 2);
t = null;
gc();
assertEq(finalizeCount(), 3);

// Once all of an instance's elements in a Table have been clobbered, the
// Instance should not be reachable.
resetFinalizeCount();
var i1 = wasmEvalText(`(module (func $f1 (result i32) (i32.const 13)) (export "f1" $f1))`);
var i2 = wasmEvalText(`(module (func $f2 (result i32) (i32.const 42)) (export "f2" $f2))`);
var f1 = i1.exports.f1;
var f2 = i2.exports.f2;
var t = new Table({initial:2, element:"funcref"});
i1.edge = makeFinalizeObserver();
i2.edge = makeFinalizeObserver();
f1.edge = makeFinalizeObserver();
f2.edge = makeFinalizeObserver();
t.edge = makeFinalizeObserver();
t.set(0, f1);
t.set(1, f2);
gc();
assertEq(finalizeCount(), 0);
f1 = f2 = null;
i1.exports = null;
i2.exports = null;
gc();
assertEq(finalizeCount(), 0);
i1 = null;
i2 = null;
gc();
assertEq(finalizeCount(), 0);
t.set(0, t.get(1));
gc();
assertEq(finalizeCount(), 2);
t.set(0, null);
t.set(1, null);
gc();
assertEq(finalizeCount(), 4);
t = null;
gc();
assertEq(finalizeCount(), 5);

// Ensure that an instance that is only live on the stack cannot be GC even if
// there are no outstanding references.
resetFinalizeCount();
const N = 10;
var tbl = new Table({initial:N, element:"funcref"});
tbl.edge = makeFinalizeObserver();
function runTest() {
    tbl = null;
    gc();
    assertEq(finalizeCount(), 0);
    return 100;
}
var i = wasmEvalText(
    `(module
        (import $imp "a" "b" (result i32))
        (func $f (param i32) (result i32) (call $imp))
        (export "f" $f)
    )`,
    {a:{b:runTest}}
);
i.edge = makeFinalizeObserver();
tbl.set(0, i.exports.f);
var m = new Module(wasmTextToBinary(`(module
    (import "a" "b" (table ${N} funcref))
    (type $i2i (func (param i32) (result i32)))
    (func $f (param $i i32) (result i32)
        (local.set $i (i32.sub (local.get $i) (i32.const 1)))
        (i32.add
            (i32.const 1)
            (call_indirect $i2i (local.get $i) (local.get $i))))
    (export "f" $f)
)`));
for (var i = 1; i < N; i++) {
    var inst = new Instance(m, {a:{b:tbl}});
    inst.edge = makeFinalizeObserver();
    tbl.set(i, inst.exports.f);
}
inst = null;
assertEq(tbl.get(N - 1)(N - 1), 109);
gc();
assertEq(finalizeCount(), N + 1);
