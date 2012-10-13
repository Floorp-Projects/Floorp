// In a debuggee with functions, findScripts finds those functions' scripts.

var g = newGlobal('new-compartment');
g.eval('function f(){}');
g.eval('function g(){}');
g.eval('function h(){}');

var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var fw = gw.makeDebuggeeValue(g.f);
var gw = gw.makeDebuggeeValue(g.g);
var hw = gw.makeDebuggeeValue(g.h);

assertEq(dbg.findScripts().indexOf(fw.script) != -1, true);
assertEq(dbg.findScripts().indexOf(gw.script) != -1, true);
assertEq(dbg.findScripts().indexOf(hw.script) != -1, true);
