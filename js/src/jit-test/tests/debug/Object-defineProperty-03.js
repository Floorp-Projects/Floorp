// defineProperty can set array elements

var g = newGlobal();
g.a = g.Array(0, 1, 2);
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var aw = gw.getOwnPropertyDescriptor("a").value;

aw.defineProperty(0, {value: 'ok0'});  // by number
assertEq(g.a[0], 'ok0');
var desc = g.Object.getOwnPropertyDescriptor(g.a, "0");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);

aw.defineProperty("1", {value: 'ok1'});  // by string
assertEq(g.a[1], 'ok1');
desc = g.Object.getOwnPropertyDescriptor(g.a, "1");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, true);
assertEq(desc.writable, true);
