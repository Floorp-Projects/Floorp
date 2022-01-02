// |jit-test| test-also=--wasm-compiler=optimizing; exitstatus: 3; skip-if: !wasmDebuggingEnabled()
// Checking in debug frame is initialized properly during stack overflow.

var dbg;
(function () { dbg = new (newGlobal().Debugger)(this); })();

var m = new WebAssembly.Module(wasmTextToBinary(`(module
    (import "a" "b" (result f64))
    ;; function that allocated large space for locals on the stack
    (func (export "f") ${Array(200).join("(param f64)")} (result f64) (call 0))
)`));
var f = () => i.exports.f();
var i = new WebAssembly.Instance(m, {a: {b: f}});
f();
