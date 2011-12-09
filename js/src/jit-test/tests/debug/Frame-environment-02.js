// dbg.getNewestFrame().environment works.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.h = function () {
    var env = dbg.getNewestFrame().environment;
    assertEq(env instanceof Debugger.Environment, true);
    assertEq(env.object, gw);
    assertEq(env.parent, null);
};
g.eval("h()");
