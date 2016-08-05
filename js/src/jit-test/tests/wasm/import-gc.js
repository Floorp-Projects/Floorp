// |jit-test| --no-baseline
// Turn off baseline and since it messes up the GC finalization assertions by
// adding spurious edges to the GC graph.

load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const m1 = new Module(textToBinary(`(module (func) (export "f" 0))`));
const m2 = new Module(textToBinary(`(module (import "a" "f") (func) (export "g" 0))`));

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
assertEq(finalizeCount(), 1);
i2 = null;
gc();
assertEq(finalizeCount(), 1);
i1 = null;
gc();
assertEq(finalizeCount(), 1);
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
assertEq(finalizeCount(), 1);
i2 = null;
gc();
assertEq(finalizeCount(), 2);
i1.exports = null;
f = null;
gc();
assertEq(finalizeCount(), 3);
i1 = null;
gc();
assertEq(finalizeCount(), 4);
