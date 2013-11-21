// Source.prototype.elementProperty can be a string or undefined.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.evaluate("function f(x) { return 2*x; }", {elementProperty: "src"});
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.elementProperty, "src");
g.evaluate("function f(x) { return 2*x; }");
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.elementProperty, undefined);
