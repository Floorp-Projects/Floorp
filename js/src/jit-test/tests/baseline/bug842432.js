// |jit-test| error: fff is not
var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval('function f(n) { if (n > 0) f(n-1); }');

dbg.onEnterFrame = function(frame) {
    frame.onPop = function() {
        fff();
    };
};

g.f(5);
