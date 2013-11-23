// Source.prototype.element can be an object or undefined.

var g = newGlobal();
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.evaluate("function f(x) { return 2*x; }", {element: { foo: "bar" }});
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(typeof fw.script.source.element, "object");
assertEq(fw.script.source.element instanceof Debugger.Object, true);
assertEq(fw.script.source.element.getOwnPropertyDescriptor("foo").value, "bar");
g.evaluate("function f(x) { return 2*x; }");
var fw = gw.getOwnPropertyDescriptor('f').value;
assertEq(typeof fw.script.source.element, "undefined");
