// source.element is undefined if the script was introduced using eval() or Function().

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

g.eval("function f(x) { return 2*x; }");
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.element, undefined);

g.x = g.Function("return 13;");
var xw = gw.getOwnPropertyDescriptor('x').value;
assertEq(xw.script.source.element, undefined);
