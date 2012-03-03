// In a debugger with multiple debuggees, findScripts finds scripts across all debuggees.
var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');
var dbg = new Debugger(g1, g2);

g1.eval('function f() {}');
g2.eval('function g() {}');

var scripts = dbg.findScripts();
assertEq(scripts.indexOf(dbg.addDebuggee(g1.f).script) != -1, true);
assertEq(scripts.indexOf(dbg.addDebuggee(g2.g).script) != -1, true);
