// Binary: cache/js-dbg-64-f189dd6316eb-linux
// Flags:
//

var g = newGlobal('new-compartment');
g.eval("var a = {};");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var desc = gw.getOwnPropertyDescriptor("a");
gw.defineProperty("b", desc);
Debugger(g.a, g.b);
