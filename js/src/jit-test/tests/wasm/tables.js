// |jit-test| test-also-wasm-baseline
load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const evalText = (str, imports) => new Instance(new Module(textToBinary(str)), imports);

var callee = i => `(func $f${i} (result i32) (i32.const ${i}))`;

assertErrorMessage(() => new Module(textToBinary(`(module (elem (i32.const 0) $f0) ${callee(0)})`)), TypeError, /table index out of range/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem (i32.const 0) 0))`)), TypeError, /table element out of range/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (func) (elem (i32.const 0) 0 1))`)), TypeError, /table element out of range/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (func) (elem (f32.const 0) 0) ${callee(0)})`)), TypeError, /type mismatch/);

assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem (i32.const 10) $f0) ${callee(0)})`)), TypeError, /element segment does not fit/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem (i32.const 8) $f0 $f0 $f0) ${callee(0)})`)), TypeError, /element segment does not fit/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem (i32.const 1) $f0 $f0) (elem (i32.const 0) $f0) ${callee(0)})`)), TypeError, /must be.*ordered/);
assertErrorMessage(() => new Module(textToBinary(`(module (table (resizable 10)) (elem (i32.const 1) $f0 $f0) (elem (i32.const 2) $f0) ${callee(0)})`)), TypeError, /must be.*disjoint/);

assertErrorMessage(() => evalText(`(module (table (resizable 10)) (import "globals" "a" (global i32 immutable)) (elem (get_global 0) $f0) ${callee(0)})`, {globals:{a:10}}), TypeError, /element segment does not fit/);
assertErrorMessage(() => evalText(`(module (table (resizable 10)) (import "globals" "a" (global i32 immutable)) (elem (get_global 0) $f0 $f0 $f0) ${callee(0)})`, {globals:{a:8}}), TypeError, /element segment does not fit/);
assertErrorMessage(() => evalText(`(module (table (resizable 10)) (import "globals" "a" (global i32 immutable)) (elem (i32.const 1) $f0 $f0) (elem (get_global 0) $f0) ${callee(0)})`, {globals:{a:0}}), TypeError, /must be.*ordered/);
assertErrorMessage(() => evalText(`(module (table (resizable 10)) (import "globals" "a" (global i32 immutable)) (elem (get_global 0) $f0 $f0) (elem (i32.const 2) $f0) ${callee(0)})`, {globals:{a:1}}), TypeError, /must be.*disjoint/);

var tbl = new Table({initial:50});
assertErrorMessage(() => evalText(`(module
    (import "globals" "table" (table 10 100))
    (import "globals" "a" (global i32 immutable))
    (elem (get_global 0) $f0 $f0)
    ${callee(0)})
`, {globals:{a:20, table:tbl}}), Error, /element segment does not fit/);

var caller = `(type $v2i (func (result i32))) (func $call (param $i i32) (result i32) (call_indirect $v2i (get_local $i))) (export "call" $call)`
var callee = i => `(func $f${i} (type $v2i) (result i32) (i32.const ${i}))`;

var call = evalText(`(module (table (resizable 10)) ${callee(0)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem (i32.const 0)) ${callee(0)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem (i32.const 0) $f0) ${callee(0)} ${caller})`).exports.call;
assertEq(call(0), 0);
assertErrorMessage(() => call(1), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(2), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var call = evalText(`(module (table (resizable 10)) (elem (i32.const 1) $f0 $f1) (elem (i32.const 4) $f0 $f2) ${callee(0)} ${callee(1)} ${callee(2)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), Error, /bad wasm indirect call/);
assertEq(call(1), 0);
assertEq(call(2), 1);
assertErrorMessage(() => call(3), Error, /bad wasm indirect call/);
assertEq(call(4), 0);
assertEq(call(5), 2);
assertErrorMessage(() => call(6), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(10), Error, /out-of-range/);

var tbl = new Table({initial:3});
var call = evalText(`(module (import "a" "b" (table 2)) (export "tbl" table) (elem (i32.const 0) $f0 $f1) ${callee(0)} ${callee(1)} ${caller})`, {a:{b:tbl}}).exports.call;
assertEq(call(0), 0);
assertEq(call(1), 1);
assertEq(tbl.get(0)(), 0);
assertEq(tbl.get(1)(), 1);

// Call signatures are matched structurally:

var call = evalText(`(module
    (type $v2i1 (func (result i32)))
    (type $v2i2 (func (result i32)))
    (type $i2v (func (param i32)))
    (table $a $b $c)
    (func $a (type $v2i1) (result i32) (i32.const 0))
    (func $b (type $v2i2) (result i32) (i32.const 1))
    (func $c (type $i2v) (param i32))
    (func $call (param i32) (result i32) (call_indirect $v2i1 (get_local 0)))
    (export "call" $call)
)`).exports.call;
assertEq(call(0), 0);
assertEq(call(1), 1);
assertErrorMessage(() => call(2), Error, /bad wasm indirect call/);
