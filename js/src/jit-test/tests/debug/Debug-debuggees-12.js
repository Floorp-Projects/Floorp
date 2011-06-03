// Events in a non-debuggee are ignored, even if a debuggee is in the same compartment.
var g1 = newGlobal('new-compartment');
var g2 = g1.eval("newGlobal('same-compartment')");
var dbg = new Debug(g1);
var hits = 0;
dbg.hooks = {debuggerHandler: function () { hits++; }};
g1.eval("debugger;");
assertEq(hits, 1);
g2.eval("debugger;");
assertEq(hits, 1);
