// |jit-test| test-also-no-wasm-baseline; exitstatus: 3
// Checking in debug frame is initialized properly during stack overflow.

if (!wasmDebuggingIsSupported())
    quit(3);

var dbg;
(function () { dbg = new (newGlobal().Debugger)(this); })();

if (!wasmIsSupported())
     throw "TestComplete";

var m = new WebAssembly.Module(wasmTextToBinary(`(module
    (import "a" "b" (result f64))
    ;; function that allocated large space for locals on the stack
    (func (export "f") ${Array(200).join("(param f64)")} (result f64) (call 0))
)`));
var f = () => i.exports.f();
var i = new WebAssembly.Instance(m, {a: {b: f}});
f();
