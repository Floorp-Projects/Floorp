// |jit-test| error: timeout

options('werror');

var g = newGlobal();
g.parent = this;
g.eval("(" + function() {
var dbg = Debugger(parent);
var handler = {hit: function() {}};

dbg.onEnterFrame = function(frame) {
    frame.onStep = function() {}
}
} + ")()");

g = newGlobal();
g.parent = this;
g.eval("Debugger(parent).onExceptionUnwind = function () {};");

function f(x) {
    if (x === 0) {
        return;
    }
    f(x - 1);
    f(x - 1);
}
timeout(0.00001);
f(100);
