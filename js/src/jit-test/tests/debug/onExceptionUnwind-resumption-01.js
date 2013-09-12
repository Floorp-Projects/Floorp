// Check that an onExceptionUnwind hook can force a frame to return a value early.

var g = newGlobal();
var dbg = Debugger(g);
dbg.onExceptionUnwind = function (frame, exc) {
    return { return:"sproon" };
};
g.eval("function f() { throw 'ksnife'; }");
assertEq(g.f(), "sproon");
