// |jit-test| error:foo
var g = newGlobal();
g.parent = this;
g.eval("(" + function() {
    var dbg = new Debugger(parent);
    count = 0;
    dbg.onExceptionUnwind = function(frame) {
        frame.onPop = function() { if (count++ < 30) frame.eval("x = 3"); };
    };
} + ")()");
Object.defineProperty(this, "x", {set: [].map});
throw "foo";
