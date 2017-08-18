// |jit-test| test-also-no-wasm-baseline; exitstatus: 3

load(libdir + "asserts.js");

if (!wasmDebuggingIsSupported())
    quit(3);

var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {  some_error; };");

var module = new WebAssembly.Module(wasmTextToBinary(`
(module
    (import $imp "a" "b" (result i32))
    (func $call (result i32) (call 0))
    (export "call" $call)
)`));

var instance = new WebAssembly.Instance(module, { a: { b: () => {
  some_other_error;
}}});
instance.exports.call();
