// In a debuggee with functions, findScripts finds those functions' scripts.

var g = newGlobal('new-compartment');
g.eval('function f(){}');
g.eval('function g(){}');
g.eval('function h(){}');

var dbg = new Debugger(g);
var fw = dbg.addDebuggee(g.f);
var gw = dbg.addDebuggee(g.g);
var hw = dbg.addDebuggee(g.h);

assertEq(dbg.findScripts().indexOf(fw.script) != -1, true);
assertEq(dbg.findScripts().indexOf(gw.script) != -1, true);
assertEq(dbg.findScripts().indexOf(hw.script) != -1, true);
