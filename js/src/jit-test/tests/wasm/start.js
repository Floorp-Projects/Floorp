assertErrorMessage(() => wasmEvalText('(module (func) (start 0) (start 0))'), SyntaxError, /wasm text error/);
assertErrorMessage(() => wasmEvalText('(module (func) (start $unknown))'), SyntaxError, /failed to find/);

wasmFailValidateText('(module (func) (start 1))', /unknown start function/);
wasmFailValidateText('(module (func (param i32)) (start 0))', /must be nullary/);
wasmFailValidateText('(module (func (param i32) (param f32)) (start 0))', /must be nullary/);
wasmFailValidateText('(module (func (param i32) (param f32) (param f64)) (start 0))', /must be nullary/);
wasmFailValidateText('(module (func (result f32)) (start 0))', /must not return anything/);

// Basic use case.
var count = 0;
function inc() { count++; }
var exports = wasmEvalText(`(module (import "" "inc" (func $imp)) (func $f (call $imp)) (start $f))`, { "":{inc} }).exports;
assertEq(count, 1);
assertEq(Object.keys(exports).length, 0);

count = 0;
exports = wasmEvalText(`(module (import "" "inc" (func)) (func $start (call 0)) (start $start) (export "" (func 0)))`, { "":{inc} }).exports;
assertEq(count, 1);
assertEq(typeof exports[""], 'function');
assertEq(exports[""](), undefined);
assertEq(count, 2);

// New API.
const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;

count = 0;
const m = new Module(wasmTextToBinary('(module (import "" "inc" (func $imp)) (func) (func $start (call $imp)) (start $start) (export "" (func $start)))'));
assertEq(count, 0);

assertErrorMessage(() => new Instance(m), TypeError, /second argument must be an object/);
assertEq(count, 0);

const i1 = new Instance(m, { "":{inc} });
assertEq(count, 1);
i1.exports[""]();
assertEq(count, 2);

const i2 = new Instance(m, { "":{inc} });
assertEq(count, 3);

function fail() { assertEq(true, false); }

count = 0;
const m2 = new Module(wasmTextToBinary('(module (import "" "fail" (func)) (import "" "inc" (func $imp)) (func) (start $imp))'));
assertEq(count, 0);
new Instance(m2, {"":{ inc, fail }});
assertEq(count, 1);
