// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

assertErrorMessage(() => wasmEvalText('(module (func) (start 0) (start 0))'), SyntaxError, /wasm text error/);
assertErrorMessage(() => wasmEvalText('(module (func) (start 1))'), TypeError, /unknown start function/);
assertErrorMessage(() => wasmEvalText('(module (func) (start $unknown))'), SyntaxError, /label.*not found/);

assertErrorMessage(() => wasmEvalText('(module (func (param i32)) (start 0))'), TypeError, /must be nullary/);
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32)) (start 0))'), TypeError, /must be nullary/);
assertErrorMessage(() => wasmEvalText('(module (func (param i32) (param f32) (param f64)) (start 0))'), TypeError, /must be nullary/);

assertErrorMessage(() => wasmEvalText('(module (func (result f32)) (start 0))'), TypeError, /must not return anything/);

// Basic use case.
var count = 0;
function inc() { count++; }
var exports = wasmEvalText(`(module (import $imp "inc" "") (func $f (param i32)) (func (call_import $imp)) (start $f))`, { inc });
assertEq(count, 1);
assertEq(Object.keys(exports).length, 0);

count = 0;
exports = wasmEvalText(`(module (import "inc" "") (func $start (call_import 0)) (start $start) (export "" 0))`, { inc });
assertEq(count, 1);
assertEq(typeof exports, 'function');
assertEq(exports(), undefined);
assertEq(count, 2);

// New API.
const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;
const textToBinary = str => wasmTextToBinary(str, 'new-format');

count = 0;
const m = new Module(textToBinary('(module (import $imp "inc" "") (func) (func $start (call $imp)) (start $start) (export "" $start))'));
assertEq(count, 0);

assertErrorMessage(() => new Instance(m), TypeError, /no import object given/);
assertEq(count, 0);

const i1 = new Instance(m, { inc });
assertEq(count, 1);
i1.exports[""]();
assertEq(count, 2);

const i2 = new Instance(m, { inc });
assertEq(count, 3);

function fail() { assertEq(true, false); }

count = 0;
const m2 = new Module(textToBinary('(module (import "fail" "") (import $imp "inc" "") (func) (start $imp))'));
assertEq(count, 0);
new Instance(m2, { inc, fail });
assertEq(count, 1);
