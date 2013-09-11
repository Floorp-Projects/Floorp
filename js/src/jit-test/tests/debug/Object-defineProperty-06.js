// obj.defineProperty with vague descriptors works like Object.defineProperty

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

gw.defineProperty("p", {configurable: true, enumerable: true});
assertEq(g.p, undefined);
var desc = g.Object.getOwnPropertyDescriptor(g, "p");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.value, undefined);
assertEq(desc.writable, false);

gw.defineProperty("q", {});
assertEq(g.q, undefined);
var desc = g.Object.getOwnPropertyDescriptor(g, "q");
assertEq(desc.configurable, false);
assertEq(desc.enumerable, false);
assertEq(desc.value, undefined);
assertEq(desc.writable, false);
