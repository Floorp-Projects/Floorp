// defineProperty can define accessor properties.

var g = newGlobal('new-compartment');
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
g.value = undefined;
g.eval("function gf() { return 12; }\n" +
       "function sf(v) { value = v; }\n");
var gfw = gw.getOwnPropertyDescriptor("gf").value;
var sfw = gw.getOwnPropertyDescriptor("sf").value;
gw.defineProperty("x", {configurable: true, get: gfw, set: sfw});
assertEq(g.x, 12);
g.x = 'ok';
assertEq(g.value, 'ok');

var desc = g.Object.getOwnPropertyDescriptor(g, "x");
assertEq(desc.configurable, true);
assertEq(desc.enumerable, false);
assertEq(desc.get, g.gf);
assertEq(desc.set, g.sf);
