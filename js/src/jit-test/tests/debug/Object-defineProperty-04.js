// defineProperty can add array elements, bumping length

var g = newGlobal('new-compartment');
g.a = g.Array(0, 1, 2);
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var aw = gw.getOwnPropertyDescriptor("a").value;
aw.defineProperty(3, {configurable: true, enumerable: true, writable: true, value: 3});
assertEq(g.a.length, 4);
