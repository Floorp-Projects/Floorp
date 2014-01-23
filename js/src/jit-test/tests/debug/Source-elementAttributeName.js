// Source.prototype.elementAttributeName can be a string or undefined.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.evaluate("function f(x) { return 2*x; }", {elementAttributeName: "src"});
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.elementAttributeName, "src");
g.evaluate("function f(x) { return 2*x; }");
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.elementAttributeName, undefined);
