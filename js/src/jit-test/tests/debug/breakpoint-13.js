// Breakpoints should be hit on scripts gotten not via Debugger.Frame.

var g = newGlobal();
g.eval("function f(x) { return x + 1; }");
// Warm up so f gets OSRed into the jits.
g.eval("for (var i = 0; i < 10000; i++) f(i);");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
var fw = gw.getOwnPropertyDescriptor("f").value;
var hits = 0;
fw.script.setBreakpoint(0, { hit: function(frame) { hits++; } });
g.eval("f(42);");
assertEq(hits, 1);
