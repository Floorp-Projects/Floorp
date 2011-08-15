// obj.defineProperty works when obj's referent is a wrapper.

var x = {};
var g = newGlobal('new-compartment');
g.x = x;
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var xw = gw.getOwnPropertyDescriptor("x").value;
xw.defineProperty("p", {configurable: true, enumerable: true, writable: true, value: gw});
assertEq(x.p, g);

var desc = Object.getOwnPropertyDescriptor(x, "p");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
assertEq(desc.value, g);
