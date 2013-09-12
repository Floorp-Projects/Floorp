// findScripts' result includes scripts for nested functions.
var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);
var log;

g.eval('function f() { return function g() { return function h() { return "Squee!"; } } }');
var fw = gw.makeDebuggeeValue(g.f);
var gw = gw.makeDebuggeeValue(g.f());
var hw = gw.makeDebuggeeValue(g.f()());

assertEq(fw.script != gw.script, true);
assertEq(fw.script != hw.script, true);

var scripts = dbg.findScripts();
assertEq(scripts.indexOf(fw.script) != -1, true);
assertEq(scripts.indexOf(gw.script) != -1, true);
assertEq(scripts.indexOf(hw.script) != -1, true);
