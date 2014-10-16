// In a debuggee with live objects, findObjects finds those objects.

var g = newGlobal();

let defObject = v => g.eval(`this.${v} = { toString: () => "[object ${v}]" }`);
defObject("a");
defObject("b");
defObject("c");

var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var aw = gw.makeDebuggeeValue(g.a);
var bw = gw.makeDebuggeeValue(g.b);
var cw = gw.makeDebuggeeValue(g.c);

assertEq(dbg.findObjects().indexOf(aw) != -1, true);
assertEq(dbg.findObjects().indexOf(bw) != -1, true);
assertEq(dbg.findObjects().indexOf(cw) != -1, true);
