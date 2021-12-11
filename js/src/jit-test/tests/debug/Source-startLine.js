// Source.prototype.startLine reflects the start line supplied when parsing.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.evaluate("function f(x) {}");
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(fw.script.source.startLine, 1);
g.evaluate("function g(x) {}", {lineNumber: 10});
var gw = gw.getOwnPropertyDescriptor('g').value;
assertEq(gw.script.source.startLine, 10);
