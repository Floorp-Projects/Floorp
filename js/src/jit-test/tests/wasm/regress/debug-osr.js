// |jit-test| skip-if: !wasmDebuggingIsSupported()
g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
        frame.older
    }
} + ")()");

let o = {};

let func = wasmEvalText(`
  (module (import "" "inc" (func $imp)) (func) (func $start (call $imp)) (start $start) (export "" (func $start)))
`, { "": { inc: function() { o = o.set; } } }).exports[""];

assertErrorMessage(func, TypeError, /(is|of) undefined/);
