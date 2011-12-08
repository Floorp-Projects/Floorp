// The objects on the environment chain are all Debugger.Environment objects.
// The environment chain ends in null.

var g = newGlobal('new-compartment')
g.eval("function f(a) { return function (b) { return function (c) { h(); return a + b + c; }; }; }");
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var n = 0;
    for (var env = dbg.getNewestFrame().environment; env !== null; env = env.parent) {
        n++;
        assertEq(env instanceof Debugger.Environment, true);
    }
    assertEq(n >= 4, true);
    hits++;
};
assertEq(g.f(5)(7)(9), 21);
assertEq(hits, 1);
