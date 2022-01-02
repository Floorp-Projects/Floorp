// |jit-test| error: TypeError

var g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    var dbg = new Debugger(parent);
    dbg.onExceptionUnwind = function(frame) {
        frame.older.onStep = function() {}
    };
} + ")()");
function f() {
    (function inner(arr) {
        inner(arr.map);
    })([]);
}
f();
