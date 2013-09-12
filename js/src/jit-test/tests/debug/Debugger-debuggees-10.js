// Allow diamonds in the graph of the compartment "debugs" relation.
var program = newGlobal();
var d1 = newGlobal();
d1.top = this;
var d2 = newGlobal();
d2.top = this;
var dbg = new Debugger(d1, d2);
d1.eval("var dbg = new Debugger(top.program)");
d2.eval("var dbg = new Debugger(top.program)");

// mess with the edges a little bit -- all this should be fine, no cycles
d1.dbg.removeDebuggee(program);
d1.dbg.addDebuggee(program);
dbg.addDebuggee(program);
d1.dbg.addDebuggee(d2);
dbg.removeDebuggee(d2);
dbg.addDebuggee(d2);

