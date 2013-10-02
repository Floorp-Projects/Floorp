// The script and environment of a non-debuggee function are null.

var g = newGlobal();
g.eval('function f() { return "from f"; }');

var dbg = new Debugger;
var gw = dbg.makeGlobalObjectReference(g);
var fw = gw.getOwnPropertyDescriptor('f').value;

// g is not a debuggee, so we can't fetch f's script or environment.
assertEq(fw.script, null);
assertEq(fw.environment, null);

// If we add g as a debuggee, we can fetch f's script and environment.
dbg.addDebuggee(g);
var fscript = fw.script;
var fenv = fw.environment;
assertEq(fscript instanceof Debugger.Script, true);
assertEq(fenv instanceof Debugger.Environment, true);

// Removing g as a debuggee makes the script and environment inaccessible again.
dbg.removeDebuggee(g);
assertEq(fw.script, null);
assertEq(fw.environment, null);
