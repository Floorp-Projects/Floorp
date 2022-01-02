// |jit-test| --no-baseline; --no-blinterp
// Turn off baseline and since it messes up the GC finalization assertions by
// adding spurious edges to the GC graph.

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;

const m1 = new Module(wasmTextToBinary(`(module (func $f) (export "f" (func $f)))`));
const m2 = new Module(wasmTextToBinary(`(module (import "a" "f" (func)) (func $f) (export "g" (func $f)))`));

// Imported instance objects should stay alive as long as any importer is alive.
resetFinalizeCount();
var i1 = new Instance(m1);
var i2 = new Instance(m2, {a:i1.exports});
var f = i1.exports.f;
var g = i2.exports.g;
i1.edge = makeFinalizeObserver();
i2.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
g.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i1.exports = null;
f = null;
gc();
assertEq(finalizeCount(), 0);
i2 = null;
gc();
assertEq(finalizeCount(), 0);
i1 = null;
gc();
assertEq(finalizeCount(), 0);
g = null;
gc();
assertEq(finalizeCount(), 4);

// ...but the importee doesn't keep the importer alive.
resetFinalizeCount();
var i1 = new Instance(m1);
var i2 = new Instance(m2, {a:i1.exports});
var f = i1.exports.f;
var g = i2.exports.g;
i1.edge = makeFinalizeObserver();
i2.edge = makeFinalizeObserver();
f.edge = makeFinalizeObserver();
g.edge = makeFinalizeObserver();
gc();
assertEq(finalizeCount(), 0);
i2.exports = null;
g = null;
gc();
assertEq(finalizeCount(), 0);
i2 = null;
gc();
assertEq(finalizeCount(), 2);
i1.exports = null;
f = null;
gc();
assertEq(finalizeCount(), 2);
i1 = null;
gc();
assertEq(finalizeCount(), 4);
