load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const evalText = (str, imports) => new Instance(new Module(textToBinary(str)), imports).exports;

const caller = `(func $call (param $i i32) (result i32) (call_indirect 0 (get_local $i))) (export "call" $call)`
const callee = i => `(func $f${i} (result i32) (i32.const ${i}))`;

assertErrorMessage(() => new Module(textToBinary(`(module (elem 0 $f0) ${callee(0)})`)), TypeError, /table index out of range/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem 10 $f0) ${callee(0)})`)), TypeError, /element segment does not fit/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem 8 $f0 $f0 $f0) ${callee(0)})`)), TypeError, /element segment does not fit/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem 1 $f0 $f0) (elem 0 $f0) ${callee(0)})`)), TypeError, /must be.*ordered/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem 1 $f0 $f0) (elem 2 $f0) ${callee(0)})`)), TypeError, /must be.*disjoint/);

var call = evalText(`(module (table (resizable 10)) ${callee(0)} ${caller})`).call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem 0) ${callee(0)} ${caller})`).call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem 0 $f0) ${callee(0)} ${caller})`).call;
assertEq(call(0), 0);
assertErrorMessage(() => call(1), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(2), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem 1 $f0 $f1) (elem 4 $f0 $f2) ${callee(0)} ${callee(1)} ${callee(2)} ${caller})`).call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertEq(call(1), 0);
assertEq(call(2), 1);
assertErrorMessage(() => call(3), Error, /bad wasm indirect call/);
assertEq(call(4), 0);
assertEq(call(5), 2);
assertErrorMessage(() => call(6), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

