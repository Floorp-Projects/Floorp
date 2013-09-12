// findScripts finds non-compile-and-go scripts

var g = newGlobal();
g.evaluate("function f(x) { return x + 1; }", {compileAndGo: false});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var s = dbg.findScripts();
var fw = gw.getOwnPropertyDescriptor("f").value;
assertEq(s.indexOf(fw.script) !== -1, true);
