// obj.defineProperty redefining an existing property leaves unspecified attributes unchanged.

var g = newGlobal();
g.p = 1;
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

gw.defineProperty("p", {value: 2});
assertEq(g.p, 2);

var desc = Object.getOwnPropertyDescriptor(g, "p");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, 2);

g.p = 3;
assertEq(g.p, 3);
