// Debugger mode can be disabled for a compartment even if it has scripts running.
var g = newGlobal();
var dbg = Debugger(g);
g.parent = this;
var n = 2;
g.eval("parent.dbg.removeDebuggee(this); parent.n += 2");
assertEq(n, 4);
