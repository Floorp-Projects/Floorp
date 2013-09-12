// obj.defineProperty can define simple data properties.

var g = newGlobal();
var dbg = new Debugger;
var gobj = dbg.addDebuggee(g);
gobj.defineProperty("x", {configurable: true, enumerable: true, writable: true, value: 'ok'});
assertEq(g.x, 'ok');

var desc = g.Object.getOwnPropertyDescriptor(g, "x");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
