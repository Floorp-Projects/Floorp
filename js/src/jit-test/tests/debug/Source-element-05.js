// source.element is undefined when bad values are passed to evaluate().

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

for (let nonObject of [32, "[object Object]", null, undefined]) {
  g.evaluate("function f(x) { return 2*x; }", {element: nonObject});
  var fw = gw.getOwnPropertyDescriptor('f').value;
  assertEq(fw.script.source.element, undefined);
}
