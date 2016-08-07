// |jit-test| test-also-wasm-baseline
load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;
const Memory = WebAssembly.Memory;

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

var tbl = new Table({initial:50, element:"anyfunc"});
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

var tbl = new Table({initial:3, element:"anyfunc"});
var call = evalText(`(module (import "a" "b" (table 3)) (export "tbl" table) (elem (i32.const 0) $f0 $f1) ${callee(0)} ${callee(1)} ${caller})`, {a:{b:tbl}}).exports.call;
assertEq(call(0), 0);
assertEq(call(1), 1);
assertEq(tbl.get(0)(), 0);
assertEq(tbl.get(1)(), 1);
assertErrorMessage(() => call(2), Error, /bad wasm indirect call/);
assertEq(tbl.get(2), null);

var exp = evalText(`(module (import "a" "b" (table 3)) (export "tbl" table) (elem (i32.const 2) $f2) ${callee(2)} ${caller})`, {a:{b:tbl}}).exports;
assertEq(exp.tbl, tbl);
assertEq(exp.call(0), 0);
assertEq(exp.call(1), 1);
assertEq(exp.call(2), 2);
assertEq(call(0), 0);
assertEq(call(1), 1);
assertEq(call(2), 2);
assertEq(tbl.get(0)(), 0);
assertEq(tbl.get(1)(), 1);
assertEq(tbl.get(2)(), 2);

var exp1 = evalText(`(module (table (resizable 10)) (export "tbl" table) (elem (i32.const 0) $f0 $f0) ${callee(0)} (export "f0" $f0) ${caller})`).exports
assertEq(exp1.tbl.get(0), exp1.f0);
assertEq(exp1.tbl.get(1), exp1.f0);
assertEq(exp1.tbl.get(2), null);
assertEq(exp1.call(0), 0);
assertEq(exp1.call(1), 0);
assertErrorMessage(() => exp1.call(2), Error, /bad wasm indirect call/);
var exp2 = evalText(`(module (import "a" "b" (table 10)) (export "tbl" table) (elem (i32.const 1) $f1 $f1) ${callee(1)} (export "f1" $f1) ${caller})`, {a:{b:exp1.tbl}}).exports
assertEq(exp1.tbl, exp2.tbl);
assertEq(exp2.tbl.get(0), exp1.f0);
assertEq(exp2.tbl.get(1), exp2.f1);
assertEq(exp2.tbl.get(2), exp2.f1);
assertEq(exp1.call(0), 0);
assertEq(exp1.call(1), 1);
assertEq(exp1.call(2), 1);
assertEq(exp2.call(0), 0);
assertEq(exp2.call(1), 1);
assertEq(exp2.call(2), 1);

var tbl = new Table({initial:3, element:"anyfunc"});
var e1 = evalText(`(module (func $f (result i32) (i32.const 42)) (export "f" $f))`).exports;
var e2 = evalText(`(module (func $g (result f32) (f32.const 10)) (export "g" $g))`).exports;
var e3 = evalText(`(module (func $h (result i32) (i32.const 13)) (export "h" $h))`).exports;
tbl.set(0, e1.f);
tbl.set(1, e2.g);
tbl.set(2, e3.h);
var e4 = evalText(`(module (import "a" "b" (table 3)) ${caller})`, {a:{b:tbl}}).exports;
assertEq(e4.call(0), 42);
assertErrorMessage(() => e4.call(1), Error, /bad wasm indirect call/);
assertEq(e4.call(2), 13);

var m = new Module(textToBinary(`(module
    (type $i2i (func (param i32) (result i32)))
    (import "a" "mem" (memory 1))
    (import "a" "tbl" (table 10))
    (import $imp "a" "imp" (result i32))
    (func $call (param $i i32) (result i32)
        (i32.add
            (call_import $imp)
            (i32.add
                (i32.load (i32.const 0))
                (if (i32.eqz (get_local $i))
                    (then (i32.const 0))
                    (else
                        (set_local $i (i32.sub (get_local $i) (i32.const 1)))
                        (call_indirect $i2i (get_local $i) (get_local $i)))))))
    (export "call" $call)
)`));
var failTime = false;
var tbl = new Table({initial:10, element:"anyfunc"});
var mem1 = new Memory({initial:1});
var e1 = new Instance(m, {a:{mem:mem1, tbl, imp() {if (failTime) throw new Error("ohai"); return 1}}}).exports;
tbl.set(0, e1.call);
var mem2 = new Memory({initial:1});
var e2 = new Instance(m, {a:{mem:mem2, tbl, imp() {return 10} }}).exports;
tbl.set(1, e2.call);
var mem3 = new Memory({initial:1});
var e3 = new Instance(m, {a:{mem:mem3, tbl, imp() {return 100} }}).exports;
new Int32Array(mem1.buffer)[0] = 1000;
new Int32Array(mem2.buffer)[0] = 10000;
new Int32Array(mem3.buffer)[0] = 100000;
assertEq(e3.call(2), 111111);
failTime = true;
assertErrorMessage(() => e3.call(2), Error, "ohai");

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

var call = evalText(`(module
    (type $A (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $B (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $C (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $D (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $E (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $F (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $G (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (table $a $b $c $d $e $f $g)
    (func $a (type $A) (get_local 7))
    (func $b (type $B) (get_local 8))
    (func $c (type $C) (get_local 9))
    (func $d (type $D) (get_local 10))
    (func $e (type $E) (get_local 11))
    (func $f (type $F) (get_local 12))
    (func $g (type $G) (get_local 13))
    (func $call (param i32) (result i32)
        (call_indirect $A (get_local 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 42)))
    (export "call" $call)
)`).exports.call;
assertEq(call(0), 42);
for (var i = 1; i < 7; i++)
    assertErrorMessage(() => call(i), Error, /bad wasm indirect call/);
assertErrorMessage(() => call(7), Error, /out-of-range/);
