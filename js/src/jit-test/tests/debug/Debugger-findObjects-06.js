// In a debugger with multiple debuggees, findObjects finds objects from all debuggees.

var g1 = newGlobal();
var g2 = newGlobal();
var dbg = new Debugger();
var g1w = dbg.addDebuggee(g1);
var g2w = dbg.addDebuggee(g2);

g1.eval('this.a = {};');
g2.eval('this.b = {};');

var objects = dbg.findObjects();
assertEq(objects.indexOf(g1w.makeDebuggeeValue(g1.a)) != -1, true);
assertEq(objects.indexOf(g2w.makeDebuggeeValue(g2.b)) != -1, true);
