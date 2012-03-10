// findScripts' result includes scripts for nested functions.
var g = newGlobal('new-compartment');
var dbg = new Debugger(g);
var log;

g.eval('function f() { return function g() { return function h() { return "Squee!"; } } }');
var fw = dbg.addDebuggee(g.f);
var gw = dbg.addDebuggee(g.f());
var hw = dbg.addDebuggee(g.f()());

assertEq(fw.script != gw.script, true);
assertEq(fw.script != hw.script, true);

var scripts = dbg.findScripts();
assertEq(scripts.indexOf(fw.script) != -1, true);
assertEq(scripts.indexOf(gw.script) != -1, true);
assertEq(scripts.indexOf(hw.script) != -1, true);
