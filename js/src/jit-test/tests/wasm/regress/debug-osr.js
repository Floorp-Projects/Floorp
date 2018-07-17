g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    Debugger(parent).onExceptionUnwind = function(frame) {
        frame.older
    }
} + ")()");

let o = {};

let func = wasmEvalText(`
  (module (import $imp "" "inc") (func) (func $start (call $imp)) (start $start) (export "" $start))
`, { "": { inc: function() { o = o.set; } } }).exports[""];

assertErrorMessage(func, TypeError, /is undefined/);
