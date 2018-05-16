// |jit-test| error: InternalError
gczeal(14);
var g = newGlobal();
g.eval('function f(a) { evaluate("f(" + " - 1);", {newContext: true}); }');
var dbg = new Debugger(g);
dbg.onEnterFrame = function(frame) {};
g.f();
