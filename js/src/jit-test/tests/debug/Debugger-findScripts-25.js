// In a debugger with multiple debuggees, findScripts finds scripts across all debuggees.
var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({newCompartment: true});
var dbg = new Debugger();
var g1w = dbg.addDebuggee(g1);
var g2w = dbg.addDebuggee(g2);

// Add eval() to make functions non-relazifiable.
g1.eval('function f() { eval(""); }');
g2.eval('function g() { eval(""); }');

var scripts = dbg.findScripts();
assertEq(scripts.indexOf(g1w.makeDebuggeeValue(g1.f).script) != -1, true);
assertEq(scripts.indexOf(g2w.makeDebuggeeValue(g2.g).script) != -1, true);
