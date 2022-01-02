// |jit-test| skip-if: !wasmDebuggingEnabled()
g = newGlobal({newCompartment: true});
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {}
} + ")()")

o = {};

let { exports } = wasmEvalText(`
  (module (import "" "inc" (func $imp)) (func) (func $start (call $imp)) (start $start) (export "" (func $start)))
`, {
    "": {
        inc: function() { o = o.p; }
    }
});

// after instanciation, the start function has been executed and o is undefined.
// This second call will throw in the imported function:

assertErrorMessage(exports[""], TypeError, /undefined/);
