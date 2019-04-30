const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const Table = WebAssembly.Table;
const Memory = WebAssembly.Memory;
const LinkError = WebAssembly.LinkError;
const RuntimeError = WebAssembly.RuntimeError;

const badFuncRefError = /can only pass WebAssembly exported functions to funcref/;

var callee = i => `(func $f${i} (result i32) (i32.const ${i}))`;

wasmFailValidateText(`(module (elem (i32.const 0) $f0) ${callee(0)})`, /elem segment requires a table section/);
wasmFailValidateText(`(module (table 10 funcref) (elem (i32.const 0) 0))`, /table element out of range/);
wasmFailValidateText(`(module (table 10 funcref) (func) (elem (i32.const 0) 0 1))`, /table element out of range/);
wasmFailValidateText(`(module (table 10 funcref) (func) (elem (f32.const 0) 0) ${callee(0)})`, /type mismatch/);

assertErrorMessage(() => wasmEvalText(`(module (table 10 funcref) (elem (i32.const 10) $f0) ${callee(0)})`), LinkError, /elem segment does not fit/);
assertErrorMessage(() => wasmEvalText(`(module (table 10 funcref) (elem (i32.const 8) $f0 $f0 $f0) ${callee(0)})`), LinkError, /elem segment does not fit/);
assertErrorMessage(() => wasmEvalText(`(module (table 0 funcref) (func) (elem (i32.const 0x10001)))`), LinkError, /elem segment does not fit/);

assertErrorMessage(() => wasmEvalText(`(module (table 10 funcref) (import "globals" "a" (global i32)) (elem (global.get 0) $f0) ${callee(0)})`, {globals:{a:10}}), LinkError, /elem segment does not fit/);
assertErrorMessage(() => wasmEvalText(`(module (table 10 funcref) (import "globals" "a" (global i32)) (elem (global.get 0) $f0 $f0 $f0) ${callee(0)})`, {globals:{a:8}}), LinkError, /elem segment does not fit/);

assertEq(new Module(wasmTextToBinary(`(module (table 10 funcref) (elem (i32.const 1) $f0 $f0) (elem (i32.const 0) $f0) ${callee(0)})`)) instanceof Module, true);
assertEq(new Module(wasmTextToBinary(`(module (table 10 funcref) (elem (i32.const 1) $f0 $f0) (elem (i32.const 2) $f0) ${callee(0)})`)) instanceof Module, true);
wasmEvalText(`(module (table 10 funcref) (import "globals" "a" (global i32)) (elem (i32.const 1) $f0 $f0) (elem (global.get 0) $f0) ${callee(0)})`, {globals:{a:0}});
wasmEvalText(`(module (table 10 funcref) (import "globals" "a" (global i32)) (elem (global.get 0) $f0 $f0) (elem (i32.const 2) $f0) ${callee(0)})`, {globals:{a:1}});

var m = new Module(wasmTextToBinary(`
    (module
        (import "globals" "table" (table 10 funcref))
        (import "globals" "a" (global i32))
        (elem (global.get 0) $f0 $f0)
        ${callee(0)})
`));
var tbl = new Table({initial:50, element:"funcref"});
assertEq(new Instance(m, {globals:{a:20, table:tbl}}) instanceof Instance, true);
assertErrorMessage(() => new Instance(m, {globals:{a:50, table:tbl}}), LinkError, /elem segment does not fit/);

var caller = `(type $v2i (func (result i32))) (func $call (param $i i32) (result i32) (call_indirect $v2i (local.get $i))) (export "call" $call)`
var callee = i => `(func $f${i} (type $v2i) (i32.const ${i}))`;

var call = wasmEvalText(`(module (table 10 funcref) ${callee(0)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), RuntimeError, /indirect call to null/);
assertErrorMessage(() => call(10), RuntimeError, /index out of bounds/);

var call = wasmEvalText(`(module (table 10 funcref) (elem (i32.const 0)) ${callee(0)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), RuntimeError, /indirect call to null/);
assertErrorMessage(() => call(10), RuntimeError, /index out of bounds/);

var call = wasmEvalText(`(module (table 10 funcref) (elem (i32.const 0) $f0) ${callee(0)} ${caller})`).exports.call;
assertEq(call(0), 0);
assertErrorMessage(() => call(1), RuntimeError, /indirect call to null/);
assertErrorMessage(() => call(2), RuntimeError, /indirect call to null/);
assertErrorMessage(() => call(10), RuntimeError, /index out of bounds/);

var call = wasmEvalText(`(module (table 10 funcref) (elem (i32.const 1) $f0 $f1) (elem (i32.const 4) $f0 $f2) ${callee(0)} ${callee(1)} ${callee(2)} ${caller})`).exports.call;
assertErrorMessage(() => call(0), RuntimeError, /indirect call to null/);
assertEq(call(1), 0);
assertEq(call(2), 1);
assertErrorMessage(() => call(3), RuntimeError, /indirect call to null/);
assertEq(call(4), 0);
assertEq(call(5), 2);
assertErrorMessage(() => call(6), RuntimeError, /indirect call to null/);
assertErrorMessage(() => call(10), RuntimeError, /index out of bounds/);

var imports = {a:{b:()=>42}};
var call = wasmEvalText(`(module (table 10 funcref) (elem (i32.const 0) $f0 $f1 $f2) ${callee(0)} (import "a" "b" (func $f1)) (import "a" "b" (func $f2 (result i32))) ${caller})`, imports).exports.call;
assertEq(call(0), 0);
assertErrorMessage(() => call(1), RuntimeError, /indirect call signature mismatch/);
assertEq(call(2), 42);

var tbl = new Table({initial:3, element:"funcref"});
var call = wasmEvalText(`(module (import "a" "b" (table 3 funcref)) (export "tbl" table) (elem (i32.const 0) $f0 $f1) ${callee(0)} ${callee(1)} ${caller})`, {a:{b:tbl}}).exports.call;
assertEq(call(0), 0);
assertEq(call(1), 1);
assertEq(tbl.get(0)(), 0);
assertEq(tbl.get(1)(), 1);
assertErrorMessage(() => call(2), RuntimeError, /indirect call to null/);
assertEq(tbl.get(2), null);

var exp = wasmEvalText(`(module (import "a" "b" (table 3 funcref)) (export "tbl" table) (elem (i32.const 2) $f2) ${callee(2)} ${caller})`, {a:{b:tbl}}).exports;
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

var exp1 = wasmEvalText(`(module (table 10 funcref) (export "tbl" table) (elem (i32.const 0) $f0 $f0) ${callee(0)} (export "f0" $f0) ${caller})`).exports
assertEq(exp1.tbl.get(0), exp1.f0);
assertEq(exp1.tbl.get(1), exp1.f0);
assertEq(exp1.tbl.get(2), null);
assertEq(exp1.call(0), 0);
assertEq(exp1.call(1), 0);
assertErrorMessage(() => exp1.call(2), RuntimeError, /indirect call to null/);
var exp2 = wasmEvalText(`(module (import "a" "b" (table 10 funcref)) (export "tbl" table) (elem (i32.const 1) $f1 $f1) ${callee(1)} (export "f1" $f1) ${caller})`, {a:{b:exp1.tbl}}).exports
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

var tbl = new Table({initial:3, element:"funcref"});
var e1 = wasmEvalText(`(module (func $f (result i32) (i32.const 42)) (export "f" $f))`).exports;
var e2 = wasmEvalText(`(module (func $g (result f32) (f32.const 10)) (export "g" $g))`).exports;
var e3 = wasmEvalText(`(module (func $h (result i32) (i32.const 13)) (export "h" $h))`).exports;
tbl.set(0, e1.f);
tbl.set(1, e2.g);
tbl.set(2, e3.h);
var e4 = wasmEvalText(`(module (import "a" "b" (table 3 funcref)) ${caller})`, {a:{b:tbl}}).exports;
assertEq(e4.call(0), 42);
assertErrorMessage(() => e4.call(1), RuntimeError, /indirect call signature mismatch/);
assertEq(e4.call(2), 13);

var asmjsFun = (function() { "use asm"; function f() {} return f })();
assertEq(isAsmJSFunction(asmjsFun), isAsmJSCompilationAvailable());
assertErrorMessage(() => tbl.set(0, asmjsFun), TypeError, badFuncRefError);
assertErrorMessage(() => tbl.grow(1, asmjsFun), TypeError, badFuncRefError);

var m = new Module(wasmTextToBinary(`(module
    (type $i2i (func (param i32) (result i32)))
    (import "a" "mem" (memory 1))
    (import "a" "tbl" (table 10 funcref))
    (import $imp "a" "imp" (result i32))
    (func $call (param $i i32) (result i32)
        (i32.add
            (call $imp)
            (i32.add
                (i32.load (i32.const 0))
                (if i32 (i32.eqz (local.get $i))
                    (then (i32.const 0))
                    (else
                        (local.set $i (i32.sub (local.get $i) (i32.const 1)))
                        (call_indirect $i2i (local.get $i) (local.get $i)))))))
    (export "call" $call)
)`));
var failTime = false;
var tbl = new Table({initial:10, element:"funcref"});
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

var call = wasmEvalText(`(module
    (type $v2i1 (func (result i32)))
    (type $v2i2 (func (result i32)))
    (type $i2v (func (param i32)))
    (table funcref (elem $a $b $c))
    (func $a (type $v2i1) (i32.const 0))
    (func $b (type $v2i2) (i32.const 1))
    (func $c (type $i2v))
    (func $call (param i32) (result i32) (call_indirect $v2i1 (local.get 0)))
    (export "call" $call)
)`).exports.call;
assertEq(call(0), 0);
assertEq(call(1), 1);
assertErrorMessage(() => call(2), RuntimeError, /indirect call signature mismatch/);

var call = wasmEvalText(`(module
    (type $A (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $B (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $C (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $D (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $E (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $F (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (type $G (func (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (param i32) (result i32)))
    (table funcref (elem $a $b $c $d $e $f $g))
    (func $a (type $A) (local.get 7))
    (func $b (type $B) (local.get 8))
    (func $c (type $C) (local.get 9))
    (func $d (type $D) (local.get 10))
    (func $e (type $E) (local.get 11))
    (func $f (type $F) (local.get 12))
    (func $g (type $G) (local.get 13))
    (func $call (param i32) (result i32)
        (call_indirect $A (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 42) (local.get 0)))
    (export "call" $call)
)`).exports.call;
assertEq(call(0), 42);
for (var i = 1; i < 7; i++)
    assertErrorMessage(() => call(i), RuntimeError, /indirect call signature mismatch/);
assertErrorMessage(() => call(7), RuntimeError, /index out of bounds/);

// Function identity isn't lost:
var tbl = wasmEvalText(`(module (table (export "tbl") funcref (elem $f)) (func $f))`).exports.tbl;
tbl.get(0).foo = 42;
gc();
assertEq(tbl.get(0).foo, 42);

(function testCrossRealmCall() {
    var g = newGlobal({sameCompartmentAs: this});

    // The memory.size builtin asserts cx->realm matches instance->realm so
    // we call it here.
    var src = `
        (module
            (import "a" "t" (table 3 funcref))
            (import "a" "m" (memory 1))
            (func $f (result i32) (i32.add (i32.const 3) (memory.size)))
            (elem (i32.const 0) $f))
    `;
    g.mem = new Memory({initial:4});
    g.tbl = new Table({initial:3, element:"funcref"});
    var i1 = g.evaluate("new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`" + src + "`)), {a:{t:tbl,m:mem}})");

    var call = new Instance(new Module(wasmTextToBinary(`
        (module
            (import "a" "t" (table 3 funcref))
            (import "a" "m" (memory 1))
            (type $v2i (func (result i32)))
            (func $call (param $i i32) (result i32) (i32.add (call_indirect $v2i (local.get $i)) (memory.size)))
            (export "call" $call))
    `)), {a:{t:g.tbl,m:g.mem}}).exports.call;

    for (var i = 0; i < 10; i++)
        assertEq(call(0), 11);
})();


// Test active segments with a table index.

{
    function makeIt(flag, tblindex) {
        return new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                               0x01, 0x00, 0x00, 0x00,
                               0x04,                   // Table section
                               0x04,                   // Section size
                               0x01,                   // One table
                               0x70,                   // Type: FuncRef
                               0x00,                   // Limits: Min only
                               0x01,                   // Limits: Min
                               0x09,                   // Elements section
                               0x07,                   // Section size
                               0x01,                   // One element segment
                               flag,                   // Flag should be 2, or > 2 if invalid
                               tblindex,               // Table index must be 0, or > 0 if invalid
                               0x41,                   // Init expr: i32.const
                               0x00,                   // Init expr: zero (payload)
                               0x0b,                   // Init expr: end
                               0x00]);                 // Zero functions
    }

    // Should succeed because this is what an active segment with index looks like
    new WebAssembly.Module(makeIt(0x02, 0x00));

    // Should fail because the kind is unknown
    assertErrorMessage(() => new WebAssembly.Module(makeIt(0x03, 0x00)),
                       WebAssembly.CompileError,
                       /invalid elem initializer-kind/);

    // Should fail because the table index is invalid
    assertErrorMessage(() => new WebAssembly.Module(makeIt(0x02, 0x01)),
                       WebAssembly.CompileError,
                       /table index out of range/);
}
