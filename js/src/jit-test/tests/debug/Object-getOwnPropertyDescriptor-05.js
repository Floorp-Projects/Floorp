// obj.getOwnPropertyDescriptor presents getters and setters as Debugger.Object objects.

var g = newGlobal('new-compartment');
g.S = function foreignFunction(v) {};
g.eval("var a = {};\n" +
       "function G() {}\n" +
       "Object.defineProperty(a, 'p', {get: G, set: S})");

var dbg = new Debugger;
var gdo = dbg.addDebuggee(g);
var desc = gdo.getOwnPropertyDescriptor("a").value.getOwnPropertyDescriptor("p");
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
assertEq("value" in desc, false);
assertEq("writable" in desc, false);
assertEq(desc.get, gdo.getOwnPropertyDescriptor("G").value);
assertEq(desc.set, gdo.getOwnPropertyDescriptor("S").value);
