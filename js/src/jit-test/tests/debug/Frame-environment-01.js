// frame.environment is a Debugger.Environment object

var g = newGlobal('new-compartment')
var dbg = Debugger(g);
g.h = function () {
    assertEq(dbg.getNewestFrame().environment instanceof Debugger.Environment, true);
};

g.eval("h()");
g.evaluate("h()");
g.eval("eval('h()')");
g.eval("function f() { h(); }");
g.f();
