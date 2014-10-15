// findObjects can filter objects by class name.

var g = newGlobal();

var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

g.eval('this.re = /foo/;');
g.eval('this.d = new Date();');

var rew = gw.makeDebuggeeValue(g.re);
var dw = gw.makeDebuggeeValue(g.d);

var objects;

objects = dbg.findObjects({ class: "RegExp" });
assertEq(objects.indexOf(rew) != -1, true);
assertEq(objects.indexOf(dw) == -1, true);

objects = dbg.findObjects({ class: "Date" });
assertEq(objects.indexOf(dw) != -1, true);
assertEq(objects.indexOf(rew) == -1, true);
