// Test that getting a function's environment can unlazify scripts.

var g = newGlobal();
g.eval('function f() { }');
var dbg = new Debugger;
var gw = dbg.makeGlobalObjectReference(g);
var fw = gw.getOwnPropertyDescriptor('f').value;
gc();
dbg.addDebuggee(g);
var fenv = fw.environment;
