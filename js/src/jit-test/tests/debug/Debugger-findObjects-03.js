// findObjects' result includes objects referenced by other objects.

var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval('this.a = { b: {} };');

var bw = gw.makeDebuggeeValue(g.a.b);

var objects = dbg.findObjects();
assertEq(objects.indexOf(bw) != -1, true);
