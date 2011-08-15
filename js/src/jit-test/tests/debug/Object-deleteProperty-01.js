// Basic deleteProperty tests.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

assertEq(gw.deleteProperty("no such property"), true);

g.Object.defineProperty(g, "p", {configurable: true, value: 0});
assertEq(gw.deleteProperty("p"), true);

g[0] = 0;
assertEq(gw.deleteProperty(0), true);
assertEq("0" in g, false);

assertEq(gw.deleteProperty(), false);  // can't delete g.undefined
assertEq(g.undefined, undefined);
