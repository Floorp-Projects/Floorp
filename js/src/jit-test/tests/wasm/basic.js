load(libdir + "wasm.js");

if (!this.wasmEval)
    quit();

function mismatchError(expect, actual) {
    return /type mismatch/;
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
assertErrorMessage(() => wasmEvalText('(module (func) (export "a" 1))'), Error, /export function index out of range/);
assertErrorMessage(() => wasmEvalText('(module (func) (func) (export "a" 2))'), Error, /export function index out of range/);

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

assertErrorMessage(() => wasmEvalText('(module (func) (export "a" 0) (export "a" 0))'), Error, /duplicate export/);
assertErrorMessage(() => wasmEvalText('(module (func) (func) (export "a" 0) (export "a" 1))'), Error, /duplicate export/);

// ----------------------------------------------------------------------------
// signatures

assertErrorMessage(() => wasmEvalText('(module (func (result i32)))'), Error, mismatchError("i32", "void"));
assertErrorMessage(() => wasmEvalText('(module (func (result i32) (nop)))'), Error, mismatchError("i32", "void"));
wasmEvalText('(module (func (nop)))');
wasmEvalText('(module (func (result i32) (i32.const 42)))');
wasmEvalText('(module (func (param i32)))');
wasmEvalText('(module (func (param i32) (result i32) (i32.const 42)))');
wasmEvalText('(module (func (result i32) (param i32) (i32.const 42)))');

