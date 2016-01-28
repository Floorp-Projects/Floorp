load(libdir + "wasm.js");

if (!wasmIsSupported())
    quit();

function mismatchError(actual, expect) {
    var str = "type mismatch: expression has type " + actual + " but expected " + expect;
    return RegExp(str);
}

// ----------------------------------------------------------------------------
// exports

var o = wasmEvalText('(module)');
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func))');
assertEq(Object.getOwnPropertyNames(o).length, 0);

var o = wasmEvalText('(module (func) (export "" 0))');
assertEq(typeof o, "function");
assertEq(o.name, "0");
assertEq(o.length, 0);
assertEq(o(), undefined);

var o = wasmEvalText('(module (func) (export "a" 0))');
var names = Object.getOwnPropertyNames(o);
assertEq(names.length, 1);
assertEq(names[0], 'a');
var desc = Object.getOwnPropertyDescriptor(o, 'a');
assertEq(typeof desc.value, "function");
assertEq(desc.value.name, "0");
assertEq(desc.value.length, 0);
assertEq(desc.value(), undefined);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.value(), undefined);

var o = wasmEvalText('(module (func) (func) (export "" 0) (export "a" 1))');
assertEq(typeof o, "function");
assertEq(o.name, "0");
assertEq(o.length, 0);
assertEq(o(), undefined);
var desc = Object.getOwnPropertyDescriptor(o, 'a');
assertEq(typeof desc.value, "function");
assertEq(desc.value.name, "1");
assertEq(desc.value.length, 0);
assertEq(desc.value(), undefined);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq(desc.value(), undefined);

wasmEvalText('(module (func) (func) (export "a" 0))');
wasmEvalText('(module (func) (func) (export "a" 1))');
assertErrorMessage(() => wasmEvalText('(module (func) (export "a" 1))'), TypeError, /export function index out of range/);
assertErrorMessage(() => wasmEvalText('(module (func) (func) (export "a" 2))'), TypeError, /export function index out of range/);

var o = wasmEvalText('(module (func) (export "a" 0) (export "b" 0))');
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "0");
assertEq(o.a === o.b, true);

var o = wasmEvalText('(module (func) (func) (export "a" 0) (export "b" 1))');
assertEq(Object.getOwnPropertyNames(o).sort().toString(), "a,b");
assertEq(o.a.name, "0");
assertEq(o.b.name, "1");
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

assertErrorMessage(() => wasmEvalText('(module (func (param i64)))'), TypeError, /NYI/);
assertErrorMessage(() => wasmEvalText('(module (func (result i64)))'), TypeError, /NYI/);

// ----------------------------------------------------------------------------
// imports

assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', 1), Error, /Second argument, if present, must be an Object/);
assertErrorMessage(() => wasmEvalText('(module (import "a" "b"))', null), Error, /Second argument, if present, must be an Object/);

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

assertErrorMessage(() => wasmEvalText('(module (func (local i64)))'), TypeError, /NYI/);

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

