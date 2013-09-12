// obj.getOwnPropertyDescriptor works on accessor properties.

var g = newGlobal();
var dbg = new Debugger;
var gdo = dbg.addDebuggee(g);

g.called = false;
g.eval("var a = {get x() { called = true; }};");

var desc = gdo.getOwnPropertyDescriptor("a").value.getOwnPropertyDescriptor("x");
assertEq(g.called, false);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, true);
assertEq("value" in desc, false);
assertEq("writable" in desc, false);
assertEq(desc.get instanceof Debugger.Object, true);
assertEq(desc.get.class, "Function");
assertEq(desc.set, undefined);
