// Redundant non-global Debugger() arguments are ignored.

var g = newGlobal('new-compartment');
g.eval("var a = {}, b = {};");
var dbg = Debugger(g.a, g.b);
var arr = dbg.getDebuggees();
assertEq(arr.length, 1);
assertEq(arr[0], dbg.addDebuggee(g));
