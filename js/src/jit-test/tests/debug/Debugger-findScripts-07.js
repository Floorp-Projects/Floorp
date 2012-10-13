// findScripts can filter scripts by global.
var g1 = newGlobal('new-compartment');
var g2 = newGlobal('new-compartment');
var g3 = newGlobal('new-compartment');

var dbg = new Debugger();
var g1w = dbg.addDebuggee(g1);
var g2w = dbg.addDebuggee(g2);

g1.eval('function f() {}');
g2.eval('function g() {}');
g2.eval('function h() {}');
var g1fw = g1w.makeDebuggeeValue(g1.f);
var g2gw = g2w.makeDebuggeeValue(g2.g);

var scripts;

scripts = dbg.findScripts({});
assertEq(scripts.indexOf(g1fw.script) != -1, true);
assertEq(scripts.indexOf(g2gw.script) != -1, true);

scripts = dbg.findScripts({global: g1});
assertEq(scripts.indexOf(g1fw.script) != -1, true);
assertEq(scripts.indexOf(g2gw.script) != -1, false);

scripts = dbg.findScripts({global: g2});
assertEq(scripts.indexOf(g1fw.script) != -1, false);
assertEq(scripts.indexOf(g2gw.script) != -1, true);

scripts = dbg.findScripts({global: g3});
// findScripts should only return debuggee scripts, and g3 isn't a
// debuggee, so this should be completely empty.
assertEq(scripts.length, 0);
