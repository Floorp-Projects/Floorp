load(libdir + "wasm.js");

// ----------------------------------------------------------------------------
// exports

var o = wasmEvalText('(module)');
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func))');
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func) (export "" 0))');
assertEq(typeof o, "function");
assertEq(o.name, "wasm-function[0]");
assertEq(o.length, 0);
assertEq(o(), undefined);

var o = wasmEvalText('(module (func) (export "a" 0))');
var names = Object.getOwnPropertyNames(o);
assertEq(names.length, 1);
assertEq(names[0], 'a');
var desc = Object.getOwnPropertyDescriptor(o, 'a');
assertEq(typeof desc.value, "function");
assertEq(desc.value.name, "wasm-function[0]");
assertEq(desc.value.length, 0);
assertEq(desc.value(), undefined);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.value(), undefined);

var o = wasmEvalText('(module (func) (func) (export "" 0) (export "a" 1))');
assertEq(typeof o, "function");
assertEq(o.name, "wasm-function[0]");
assertEq(o.length, 0);
assertEq(o(), undefined);
var desc = Object.getOwnPropertyDescriptor(o, 'a');
assertEq(typeof desc.value, "function");
assertEq(desc.value.name, "wasm-function[1]");
assertEq(desc.value.length, 0);
assertEq(desc.value(), undefined);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.value(), undefined);

wasmEvalText('(module (func) (func) (export "a" 0))');
wasmEvalText('(module (func) (func) (export "a" 1))');
wasmEvalText('(module (func $a) (func $b) (export "a" $a) (export "b" $b))');
wasmEvalText('(module (func $a) (func $b) (export "a" $a) (export "b" $b))');

assertErrorMessage(() => wasmEvalText('(module (func) (export "a" 1))'), TypeError, /export function index out of range/);
assertErrorMessage(() => wasmEvalText('(module (func) (func) (export "a" 2))'), TypeError, /export function index out of range/);

var o = wasmEvalText('(module (func) (export "a" 0) (export "b" 0))');
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "wasm-function[0]");
assertEq(o.b.name, "wasm-function[0]");
assertEq(o.a === o.b, true);

var o = wasmEvalText('(module (func) (func) (export "a" 0) (export "b" 1))');
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "wasm-function[0]");
assertEq(o.b.name, "wasm-function[1]");
assertEq(o.a === o.b, false);

var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" 0) (export "b" 1))');
assertEq(o.a(), 1);
assertEq(o.b(), 2);
var o = wasmEvalText('(module (func (result i32) (i32.const 1)) (func (result i32) (i32.const 2)) (export "a" 1) (export "b" 0))');
assertEq(o.a(), 2);
assertEq(o.b(), 1);

assertErrorMessage(() => wasmEvalText('(module (func) (export "a" 0) (export "a" 0))'), TypeError, /duplicate export/);
assertErrorMessage(() => wasmEvalText('(module (func) (func) (export "a" 0) (export "a" 1))'), TypeError, /duplicate export/);

// ----------------------------------------------------------------------------
// signatures

assertErrorMessage(() => wasmEvalText('(module (func (result i32)))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (nop)))'), TypeError, mismatchError("void", "i32"));
wasmEvalText('(module (func (nop)))');
wasmEvalText('(module (func (result i32) (i32.const 42)))');
wasmEvalText('(module (func (param i32)))');
wasmEvalText('(module (func (param i32) (result i32) (i32.const 42)))');
wasmEvalText('(module (func (result i32) (param i32) (i32.const 42)))');
wasmEvalText('(module (func (param f32)))');
wasmEvalText('(module (func (param f64)))');

if (!hasI64()) {
    assertErrorMessage(() => wasmEvalText('(module (func (param i64)))'), TypeError, /NYI/);
    assertErrorMessage(() => wasmEvalText('(module (func (result i64)))'), TypeError, /NYI/);
    assertErrorMessage(() => wasmEvalText('(module (func (result i32) (i32.wrap/i64 (i64.add (i64.const 1) (i64.const 2)))))'), TypeError, /NYI/);
} else {
    assertErrorMessage(() => wasmEvalText('(module (func (param i64) (result i32) (i32.const 123)) (export "" 0))'), TypeError, /i64 argument/);
    assertErrorMessage(() => wasmEvalText('(module (func (param i32) (result i64) (i64.const 123)) (export "" 0))'), TypeError, /i64 return type/);

    setJitCompilerOption('wasm.test-mode', 1);

    assertEqI64(wasmEvalText('(module (func (result i64) (i64.const 123)) (export "" 0))')(), {low: 123, high: 0});
    assertEqI64(wasmEvalText('(module (func (param i64) (result i64) (get_local 0)) (export "" 0))')({ low: 0x7fffffff, high: 0x12340000}),
                {low: 0x7fffffff, high: 0x12340000});
    assertEqI64(wasmEvalText('(module (func (param i64) (result i64) (i64.add (get_local 0) (i64.const 1))) (export "" 0))')({ low: 0xffffffff, high: 0x12340000}), {low: 0x0, high: 0x12340001});

    setJitCompilerOption('wasm.test-mode', 0);
}

// ----------------------------------------------------------------------------
// imports

assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', 1), Error, /second argument, if present, must be an object/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', null), Error, /second argument, if present, must be an object/);

const noImportObj = /no import object given/;
const notObject = /import object field is not an Object/;
const notFunction = /import object field is not a Function/;

var code = '(module (import "a" "b"))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notObject);
assertErrorMessage(() => wasmEvalText(code, {a:{}}), TypeError, notFunction);
assertErrorMessage(() => wasmEvalText(code, {a:{b:1}}), TypeError, notFunction);
wasmEvalText(code, {a:{b:()=>{}}});

var code = '(module (import "" "b"))';
assertErrorMessage(() => wasmEvalText(code), TypeError, /module name cannot be empty/);

var code = '(module (import "a" ""))';
assertErrorMessage(() => wasmEvalText(code), TypeError, noImportObj);
assertErrorMessage(() => wasmEvalText(code, {}), TypeError, notFunction);
assertErrorMessage(() => wasmEvalText(code, {a:1}), TypeError, notFunction);
wasmEvalText(code, {a:()=>{}});

var code = '(module (import "a" "") (import "b" "c") (import "c" ""))';
assertErrorMessage(() => wasmEvalText(code, {a:()=>{}, b:{c:()=>{}}, c:{}}), TypeError, notFunction);
wasmEvalText(code, {a:()=>{}, b:{c:()=>{}}, c:()=>{}});

wasmEvalText('(module (import "a" "" (result i32)))', {a: ()=> {}});
wasmEvalText('(module (import "a" "" (result f32)))', {a: ()=> {}});
wasmEvalText('(module (import "a" "" (result f64)))', {a: ()=> {}});
wasmEvalText('(module (import $foo "a" "" (result f64)))', {a: ()=> {}});

// ----------------------------------------------------------------------------
// memory

wasmEvalText('(module (memory 0))');
wasmEvalText('(module (memory 1))');
assertErrorMessage(() => wasmEvalText('(module (memory 65536))'), TypeError, /initial memory size too big/);

// May OOM, but must not crash:
try {
    wasmEvalText('(module (memory 65535))');
} catch (e) {
    print(e);
    assertEq(String(e).indexOf("out of memory") != -1, true);
}

// Tests to reinstate pending a switch back to "real" memory exports:
//
//assertErrorMessage(() => wasmEvalText('(module (export "" memory))'), TypeError, /no memory section/);
//
//var buf = wasmEvalText('(module (memory 1) (export "" memory))');
//assertEq(buf instanceof ArrayBuffer, true);
//assertEq(buf.byteLength, 65536);
//
//assertErrorMessage(() => wasmEvalText('(module (memory 1) (export "a" memory) (export "a" memory))'), TypeError, /duplicate export/);
//assertErrorMessage(() => wasmEvalText('(module (memory 1) (func) (export "a" memory) (export "a" 0))'), TypeError, /duplicate export/);
//var {a, b} = wasmEvalText('(module (memory 1) (export "a" memory) (export "b" memory))');
//assertEq(a instanceof ArrayBuffer, true);
//assertEq(a, b);
//
//var obj = wasmEvalText('(module (memory 1) (func (result i32) (i32.const 42)) (func (nop)) (export "a" memory) (export "b" 0) (export "c" 1))');
//assertEq(obj.a instanceof ArrayBuffer, true);
//assertEq(obj.b instanceof Function, true);
//assertEq(obj.c instanceof Function, true);
//assertEq(obj.a.byteLength, 65536);
//assertEq(obj.b(), 42);
//assertEq(obj.c(), undefined);
//
//var obj = wasmEvalText('(module (memory 1) (func (result i32) (i32.const 42)) (export "" memory) (export "a" 0) (export "b" 0))');
//assertEq(obj instanceof ArrayBuffer, true);
//assertEq(obj.a instanceof Function, true);
//assertEq(obj.b instanceof Function, true);
//assertEq(obj.a, obj.b);
//assertEq(obj.byteLength, 65536);
//assertEq(obj.a(), 42);
//
//var buf = wasmEvalText('(module (memory 1 (segment 0 "")) (export "" memory))');
//assertEq(new Uint8Array(buf)[0], 0);
//
//var buf = wasmEvalText('(module (memory 1 (segment 65536 "")) (export "" memory))');
//assertEq(new Uint8Array(buf)[0], 0);
//
//var buf = wasmEvalText('(module (memory 1 (segment 0 "a")) (export "" memory))');
//assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));
//
//var buf = wasmEvalText('(module (memory 1 (segment 0 "a") (segment 2 "b")) (export "" memory))');
//assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));
//assertEq(new Uint8Array(buf)[1], 0);
//assertEq(new Uint8Array(buf)[2], 'b'.charCodeAt(0));
//
//var buf = wasmEvalText('(module (memory 1 (segment 65535 "c")) (export "" memory))');
//assertEq(new Uint8Array(buf)[0], 0);
//assertEq(new Uint8Array(buf)[65535], 'c'.charCodeAt(0));
//
//assertErrorMessage(() => wasmEvalText('(module (memory 1 (segment 65536 "a")) (export "" memory))'), TypeError, /data segment does not fit/);
//assertErrorMessage(() => wasmEvalText('(module (memory 1 (segment 65535 "ab")) (export "" memory))'), TypeError, /data segment does not fit/);

var buf = wasmEvalText('(module (memory 1) (export "memory" memory))').memory;
assertEq(buf instanceof ArrayBuffer, true);
assertEq(buf.byteLength, 65536);

var obj = wasmEvalText('(module (memory 1) (func (result i32) (i32.const 42)) (func (nop)) (export "memory" memory) (export "b" 0) (export "c" 1))');
assertEq(obj.memory instanceof ArrayBuffer, true);
assertEq(obj.b instanceof Function, true);
assertEq(obj.c instanceof Function, true);
assertEq(obj.memory.byteLength, 65536);
assertEq(obj.b(), 42);
assertEq(obj.c(), undefined);

var buf = wasmEvalText('(module (memory 1 (segment 0 "")) (export "memory" memory))').memory;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1 (segment 65536 "")) (export "memory" memory))').memory;
assertEq(new Uint8Array(buf)[0], 0);

var buf = wasmEvalText('(module (memory 1 (segment 0 "a")) (export "memory" memory))').memory;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1 (segment 0 "a") (segment 2 "b")) (export "memory" memory))').memory;
assertEq(new Uint8Array(buf)[0], 'a'.charCodeAt(0));
assertEq(new Uint8Array(buf)[1], 0);
assertEq(new Uint8Array(buf)[2], 'b'.charCodeAt(0));

var buf = wasmEvalText('(module (memory 1 (segment 65535 "c")) (export "memory" memory))').memory;
assertEq(new Uint8Array(buf)[0], 0);
assertEq(new Uint8Array(buf)[65535], 'c'.charCodeAt(0));

assertErrorMessage(() => wasmEvalText('(module (memory 1 (segment 65536 "a")) (export "memory" memory))'), TypeError, /data segment does not fit/);
assertErrorMessage(() => wasmEvalText('(module (memory 1 (segment 65535 "ab")) (export "memory" memory))'), TypeError, /data segment does not fit/);

// ----------------------------------------------------------------------------
// locals

assertEq(wasmEvalText('(module (func (param i32) (result i32) (get_local 0)) (export "" 0))')(), 0);
assertEq(wasmEvalText('(module (func (param i32) (result i32) (get_local 0)) (export "" 0))')(42), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (get_local 0)) (export "" 0))')(42, 43), 42);
assertEq(wasmEvalText('(module (func (param i32) (param i32) (result i32) (get_local 1)) (export "" 0))')(42, 43), 43);

assertErrorMessage(() => wasmEvalText('(module (func (get_local 0)))'), TypeError, /get_local index out of range/);
wasmEvalText('(module (func (local i32)))');
wasmEvalText('(module (func (local i32) (local f32)))');
assertEq(wasmEvalText('(module (func (result i32) (local i32) (get_local 0)) (export "" 0))')(), 0);
assertErrorMessage(() => wasmEvalText('(module (func (result f32) (local i32) (get_local 0)))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (local f32) (get_local 0)))'), TypeError, mismatchError("f32", "i32"));
assertEq(wasmEvalText('(module (func (result i32) (param i32) (local f32) (get_local 0)) (export "" 0))')(), 0);
assertEq(wasmEvalText('(module (func (result f32) (param i32) (local f32) (get_local 1)) (export "" 0))')(), 0);
assertErrorMessage(() => wasmEvalText('(module (func (result f32) (param i32) (local f32) (get_local 0)))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (param i32) (local f32) (get_local 1)))'), TypeError, mismatchError("f32", "i32"));

assertErrorMessage(() => wasmEvalText('(module (func (set_local 0 (i32.const 0))))'), TypeError, /set_local index out of range/);
wasmEvalText('(module (func (local i32) (set_local 0 (i32.const 0))))');
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (set_local 0 (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (local f32) (set_local 0 (nop))))'), TypeError, mismatchError("void", "f32"));
assertErrorMessage(() => wasmEvalText('(module (func (local i32) (local f32) (set_local 0 (get_local 1))))'), TypeError, mismatchError("f32", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local i32) (local f32) (set_local 1 (get_local 0))))'), TypeError, mismatchError("i32", "f32"));
wasmEvalText('(module (func (local i32) (local f32) (set_local 0 (get_local 0))))');
wasmEvalText('(module (func (local i32) (local f32) (set_local 1 (get_local 1))))');
assertEq(wasmEvalText('(module (func (result i32) (local i32) (set_local 0 (i32.const 42))) (export "" 0))')(), 42);
assertEq(wasmEvalText('(module (func (result i32) (local i32) (set_local 0 (get_local 0))) (export "" 0))')(), 0);

if (!hasI64())
    assertErrorMessage(() => wasmEvalText('(module (func (local i64)))'), TypeError, /NYI/);

assertEq(wasmEvalText('(module (func (param $a i32) (result i32) (get_local $a)) (export "" 0))')(), 0);
assertEq(wasmEvalText('(module (func (param $a i32) (local $b i32) (result i32) (block (set_local $b (get_local $a)) (get_local $b))) (export "" 0))')(42), 42);

wasmEvalText('(module (func (local i32) (local $a f32) (set_local 0 (i32.const 1)) (set_local $a (f32.const nan))))');

// ----------------------------------------------------------------------------
// blocks

assertEq(wasmEvalText('(module (func (block)) (export "" 0))')(), undefined);

assertErrorMessage(() => wasmEvalText('(module (func (result i32) (block)))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (block (block))))'), TypeError, mismatchError("void", "i32"));
assertErrorMessage(() => wasmEvalText('(module (func (local i32) (set_local 0 (block))))'), TypeError, mismatchError("void", "i32"));

assertEq(wasmEvalText('(module (func (block (block))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (result i32) (block (i32.const 42))) (export "" 0))')(), 42);
assertEq(wasmEvalText('(module (func (result i32) (block (block (i32.const 42)))) (export "" 0))')(), 42);
assertErrorMessage(() => wasmEvalText('(module (func (result f32) (block (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));

assertEq(wasmEvalText('(module (func (result i32) (block (i32.const 13) (block (i32.const 42)))) (export "" 0))')(), 42);
assertErrorMessage(() => wasmEvalText('(module (func (result f32) (param f32) (block (get_local 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));

assertEq(wasmEvalText('(module (func (result i32) (local i32) (set_local 0 (i32.const 42)) (get_local 0)) (export "" 0))')(), 42);

// ----------------------------------------------------------------------------
// calls

// TODO: Reenable when syntactic arities are added for calls
//assertThrowsInstanceOf(() => wasmEvalText('(module (func (nop)) (func (call 0 (i32.const 0))))'), TypeError);

assertThrowsInstanceOf(() => wasmEvalText('(module (func (param i32) (nop)) (func (call 0)))'), TypeError);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (param f32) (nop)) (func (call 0 (i32.const 0))))'), TypeError);
assertErrorMessage(() => wasmEvalText('(module (func (nop)) (func (call 3)))'), TypeError, /callee index out of range/);
wasmEvalText('(module (func (nop)) (func (call 0)))');
wasmEvalText('(module (func (param i32) (nop)) (func (call 0 (i32.const 0))))');
assertEq(wasmEvalText('(module (func (result i32) (i32.const 42)) (func (result i32) (call 0)) (export "" 1))')(), 42);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 0)) (export "" 0))')(), InternalError);
assertThrowsInstanceOf(() => wasmEvalText('(module (func (call 1)) (func (call 0)) (export "" 0))')(), InternalError);
wasmEvalText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (f32.const nan))))');
assertErrorMessage(() => wasmEvalText('(module (func (param i32 f32)) (func (call 0 (i32.const 0) (i32.const 0))))'), TypeError, mismatchError("i32", "f32"));

// TODO: Reenable when syntactic arities are added for calls
//assertThrowsInstanceOf(() => wasmEvalText('(module (import "a" "") (func (call_import 0 (i32.const 0))))', {a:()=>{}}), TypeError);

assertThrowsInstanceOf(() => wasmEvalText('(module (import "a" "" (param i32)) (func (call_import 0)))', {a:()=>{}}), TypeError);
assertThrowsInstanceOf(() => wasmEvalText('(module (import "a" "" (param f32)) (func (call_import 0 (i32.const 0))))', {a:()=>{}}), TypeError);
assertErrorMessage(() => wasmEvalText('(module (import "a" "") (func (call_import 1)))'), TypeError, /import index out of range/);
wasmEvalText('(module (import "a" "") (func (call_import 0)))', {a:()=>{}});
wasmEvalText('(module (import "a" "" (param i32)) (func (call_import 0 (i32.const 0))))', {a:()=>{}});

function checkF32CallImport(v) {
    assertEq(wasmEvalText('(module (import "a" "" (result f32)) (func (result f32) (call_import 0)) (export "" 0))', {a:()=>{ return v; }})(), Math.fround(v));
    wasmEvalText('(module (import "a" "" (param f32)) (func (param f32) (call_import 0 (get_local 0))) (export "" 0))', {a:x=>{ assertEq(Math.fround(v), x); }})(v);
}
checkF32CallImport(13.37);
checkF32CallImport(NaN);
checkF32CallImport(-Infinity);
checkF32CallImport(-0);
checkF32CallImport(Math.pow(2, 32) - 1);

var f = wasmEvalText('(module (import "inc" "") (func (call_import 0)) (export "" 0))', {inc:()=>counter++});
var g = wasmEvalText('(module (import "f" "") (func (block (call_import 0) (call_import 0))) (export "" 0))', {f});
var counter = 0;
f();
assertEq(counter, 1);
g();
assertEq(counter, 3);

var f = wasmEvalText('(module (import "callf" "") (func (call_import 0)) (export "" 0))', {callf:()=>f()});
assertThrowsInstanceOf(() => f(), InternalError);

var f = wasmEvalText('(module (import "callg" "") (func (call_import 0)) (export "" 0))', {callg:()=>g()});
var g = wasmEvalText('(module (import "callf" "") (func (call_import 0)) (export "" 0))', {callf:()=>f()});
assertThrowsInstanceOf(() => f(), InternalError);

var code = '(module (import "one" "" (result i32)) (import "two" "" (result i32)) (func (result i32) (i32.const 3)) (func (result i32) (i32.const 4)) (func (result i32) BODY) (export "" 2))';
var imports = {one:()=>1, two:()=>2};
assertEq(wasmEvalText(code.replace('BODY', '(call_import 0)'), imports)(), 1);
assertEq(wasmEvalText(code.replace('BODY', '(call_import 1)'), imports)(), 2);
assertEq(wasmEvalText(code.replace('BODY', '(call 0)'), imports)(), 3);
assertEq(wasmEvalText(code.replace('BODY', '(call 1)'), imports)(), 4);

assertEq(wasmEvalText(`(module (import "evalcx" "" (param i32) (result i32)) (func (result i32) (call_import 0 (i32.const 0))) (export "" 0))`, {evalcx})(), 0);

if (typeof evaluate === 'function')
    evaluate(`Wasm.instantiateModule(wasmTextToBinary('(module)')) `, { fileName: null });

if (hasI64()) {
    assertErrorMessage(() => wasmEvalText('(module (import "a" "" (param i64) (result i32)))'), TypeError, /i64 argument/);
    assertErrorMessage(() => wasmEvalText('(module (import "a" "" (result i64)))'), TypeError, /i64 return type/);

    setJitCompilerOption('wasm.test-mode', 1);

    let imp = {
        param(i64) {
            assertEqI64(i64, {
                low: 0x9abcdef0,
                high: 0x12345678
            });
            return 42;
        },
        result(i32) {
            return {
                low: 0xabcdef01,
                high: 0x12345678 + i32
            }
        },
        paramAndResult(i64) {
            assertEqI64(i64, {
                low: 0x9abcdef0,
                high: 0x12345678
            });
            i64.low = 1337;
            return i64;
        }
    }

    assertEq(wasmEvalText(`(module
        (import "param" "" (param i64) (result i32))
        (func (result i32) (call_import 0 (i64.const 0x123456789abcdef0)))
        (export "" 0))`, imp)(), 42);

    assertEqI64(wasmEvalText(`(module
        (import "result" "" (param i32) (result i64))
        (func (result i64) (call_import 0 (i32.const 3)))
        (export "" 0))`, imp)(), { low: 0xabcdef01, high: 0x1234567b });

    // Ensure the ion exit is never taken.
    let ionThreshold = 2 * getJitCompilerOptions()['ion.warmup.trigger'];
    assertEqI64(wasmEvalText(`(module
        (import "paramAndResult" "" (param i64) (result i64))
        (func (result i64) (local i32) (local i64)
         (set_local 0 (i32.const 0))
         (loop $out $in
             (set_local 1 (call_import 0 (i64.const 0x123456789abcdef0)))
             (set_local 0 (i32.add (get_local 0) (i32.const 1)))
             (if (i32.le_s (get_local 0) (i32.const ${ionThreshold})) (br $in))
         )
         (get_local 1)
        )
    (export "" 0))`, imp)(), { low: 1337, high: 0x12345678 });

    setJitCompilerOption('wasm.test-mode', 0);
} else {
    assertErrorMessage(() => wasmEvalText('(module (import "a" "" (param i64) (result i32)))'), TypeError, /NYI/);
    assertErrorMessage(() => wasmEvalText('(module (import "a" "" (result i64)))'), TypeError, /NYI/);
}

var {v2i, i2i, i2v} = wasmEvalText(`(module
    (type (func (result i32)))
    (type (func (param i32) (result i32)))
    (type (func (param i32)))
    (func (type 0) (i32.const 13))
    (func (type 0) (i32.const 42))
    (func (type 1) (i32.add (get_local 0) (i32.const 1)))
    (func (type 1) (i32.add (get_local 0) (i32.const 2)))
    (func (type 1) (i32.add (get_local 0) (i32.const 3)))
    (func (type 1) (i32.add (get_local 0) (i32.const 4)))
    (table 0 1 2 3 4 5)
    (func (param i32) (result i32) (call_indirect 0 (get_local 0)))
    (func (param i32) (param i32) (result i32) (call_indirect 1 (get_local 0) (get_local 1)))
    (func (param i32) (call_indirect 2 (get_local 0) (i32.const 0)))
    (export "v2i" 6)
    (export "i2i" 7)
    (export "i2v" 8)
)`);

const badIndirectCall = /wasm indirect call signature mismatch/;

assertEq(v2i(0), 13);
assertEq(v2i(1), 42);
assertErrorMessage(() => v2i(2), Error, badIndirectCall);
assertErrorMessage(() => v2i(3), Error, badIndirectCall);
assertErrorMessage(() => v2i(4), Error, badIndirectCall);
assertErrorMessage(() => v2i(5), Error, badIndirectCall);

assertErrorMessage(() => i2i(0), Error, badIndirectCall);
assertErrorMessage(() => i2i(1), Error, badIndirectCall);
assertEq(i2i(2, 100), 101);
assertEq(i2i(3, 100), 102);
assertEq(i2i(4, 100), 103);
assertEq(i2i(5, 100), 104);

assertErrorMessage(() => i2v(0), Error, badIndirectCall);
assertErrorMessage(() => i2v(1), Error, badIndirectCall);
assertErrorMessage(() => i2v(2), Error, badIndirectCall);
assertErrorMessage(() => i2v(3), Error, badIndirectCall);
assertErrorMessage(() => i2v(4), Error, badIndirectCall);
assertErrorMessage(() => i2v(5), Error, badIndirectCall);

{
    enableSPSProfiling();
    wasmEvalText(`(
        module
        (func (result i32) (i32.const 0))
        (func)
        (table 1 0)
        (export "" 0)
    )`)();
    disableSPSProfiling();
}

for (bad of [6, 7, 100, Math.pow(2,31)-1, Math.pow(2,31), Math.pow(2,31)+1, Math.pow(2,32)-2, Math.pow(2,32)-1]) {
    assertThrowsInstanceOf(() => v2i(bad), RangeError);
    assertThrowsInstanceOf(() => i2i(bad, 0), RangeError);
    assertThrowsInstanceOf(() => i2v(bad, 0), RangeError);
}

var {v2i, i2i, i2v} = wasmEvalText(`(module
    (type $a (func (result i32)))
    (type $b (func (param i32) (result i32)))
    (type $c (func (param i32)))
    (func $a (type $a) (i32.const 13))
    (func $b (type $a) (i32.const 42))
    (func $c (type $b) (i32.add (get_local 0) (i32.const 1)))
    (func $d (type $b) (i32.add (get_local 0) (i32.const 2)))
    (func $e (type $b) (i32.add (get_local 0) (i32.const 3)))
    (func $f (type $b) (i32.add (get_local 0) (i32.const 4)))
    (table $a $b $c $d $e $f)
    (func (param i32) (result i32) (call_indirect $a (get_local 0)))
    (func (param i32) (param i32) (result i32) (call_indirect $b (get_local 0) (get_local 1)))
    (func (param i32) (call_indirect $c (get_local 0) (i32.const 0)))
    (export "v2i" 6)
    (export "i2i" 7)
    (export "i2v" 8)
)`);

wasmEvalText('(module (func $foo (nop)) (func (call $foo)))');
wasmEvalText('(module (func (call $foo)) (func $foo (nop)))');
wasmEvalText('(module (import $bar "a" "") (func (call_import $bar)) (func $foo (nop)))', {a:()=>{}});


// ----------------------------------------------------------------------------
// select

assertErrorMessage(() => wasmEvalText('(module (func (select (i32.const 0) (i32.const 0) (f32.const 0))))'), TypeError, mismatchError("f32", "i32"));

assertEq(wasmEvalText('(module (func (select (i32.const 0) (f32.const 0) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (select (block) (i32.const 0) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (select (return) (i32.const 0) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (i32.add (i32.const 0) (select (return) (i32.const 0) (i32.const 0)))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func (select (if (i32.const 1) (i32.const 0) (f32.const 0)) (i32.const 0) (i32.const 0))) (export "" 0))')(), undefined);
assertEq(wasmEvalText('(module (func) (func (select (call 0) (call 0) (i32.const 0))) (export "" 0))')(), undefined);

(function testSideEffects() {

var numT = 0;
var numF = 0;

var imports = {
    ifTrue: () => 1 + numT++,
    ifFalse: () => -1 + numF++,
}

// Test that side-effects are applied on both branches.
var f = wasmEvalText(`
(module
 (import "ifTrue" "" (result i32))
 (import "ifFalse" "" (result i32))
 (func (result i32) (param i32)
  (select
   (call_import 0)
   (call_import 1)
   (get_local 0)
  )
 )
 (export "" 0)
)
`, imports);

assertEq(f(-1), numT);
assertEq(numT, 1);
assertEq(numF, 1);

assertEq(f(0), numF - 2);
assertEq(numT, 2);
assertEq(numF, 2);

assertEq(f(1), numT);
assertEq(numT, 3);
assertEq(numF, 3);

assertEq(f(0), numF - 2);
assertEq(numT, 4);
assertEq(numF, 4);

assertEq(f(0), numF - 2);
assertEq(numT, 5);
assertEq(numF, 5);

assertEq(f(1), numT);
assertEq(numT, 6);
assertEq(numF, 6);

})();

function testSelect(type, trueVal, falseVal) {

    var trueJS = jsify(trueVal);
    var falseJS = jsify(falseVal);

    // Always true condition
    var alwaysTrue = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 1)
      )
     )
     (export "" 0)
    )
    `, imports);

    assertEq(alwaysTrue(0), trueJS);
    assertEq(alwaysTrue(1), trueJS);
    assertEq(alwaysTrue(-1), trueJS);

    // Always false condition
    var alwaysFalse = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (i32.const 0)
      )
     )
     (export "" 0)
    )
    `, imports);

    assertEq(alwaysFalse(0), falseJS);
    assertEq(alwaysFalse(1), falseJS);
    assertEq(alwaysFalse(-1), falseJS);

    // Variable condition
    var f = wasmEvalText(`
    (module
     (func (result ${type}) (param i32)
      (select
       (${type}.const ${trueVal})
       (${type}.const ${falseVal})
       (get_local 0)
      )
     )
     (export "" 0)
    )
    `, imports);

    assertEq(f(0), falseJS);
    assertEq(f(1), trueJS);
    assertEq(f(-1), trueJS);
}

testSelect('i32', 13, 37);
testSelect('i32', Math.pow(2, 31) - 1, -Math.pow(2, 31));

testSelect('f32', Math.fround(13.37), Math.fround(19.89));
testSelect('f32', 'infinity', '-0');
testSelect('f32', 'nan', Math.pow(2, -31));

testSelect('f64', 13.37, 19.89);
testSelect('f64', 'infinity', '-0');
testSelect('f64', 'nan', Math.pow(2, -31));

if (!hasI64()) {
    assertErrorMessage(() => wasmEvalText('(module (func (select (i64.const 0) (i64.const 1) (i32.const 0))))'), TypeError, /NYI/);
} else {

    setJitCompilerOption('wasm.test-mode', 1);

    var f = wasmEvalText(`
    (module
     (func (result i64) (param i32)
      (select
       (i64.const 0xc0010ff08badf00d)
       (i64.const 0x12345678deadc0de)
       (get_local 0)
      )
     )
     (export "" 0)
    )`, imports);

    assertEqI64(f(0),  { low: 0xdeadc0de, high: 0x12345678});
    assertEqI64(f(1),  { low: 0x8badf00d, high: 0xc0010ff0});
    assertEqI64(f(-1), { low: 0x8badf00d, high: 0xc0010ff0});

    setJitCompilerOption('wasm.test-mode', 0);
}

