// Specifying a non-debuggee global in a Debugger.prototype.findScripts query should
// cause the query to return no scripts.

var g1 = newGlobal('new-compartment');
g1.eval('function f(){}');

var g2 = newGlobal('new-compartment');
g2.eval('function g(){}');

var dbg = new Debugger(g1);
assertEq(dbg.findScripts({global:g1}).length > 0, true);
assertEq(dbg.findScripts({global:g2}).length, 0);
assertEq(dbg.findScripts({global:this}).length, 0);
