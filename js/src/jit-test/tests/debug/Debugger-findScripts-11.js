// Debugger.prototype.findScripts can filter scripts by line number.
var g = newGlobal();
var dbg = new Debugger();
var gw = dbg.addDebuggee(g);

var scriptname = scriptdir + 'Debugger-findScripts-11-script2';
g.load(scriptname);

var gfw = gw.makeDebuggeeValue(g.f);
var ggw = gw.makeDebuggeeValue(g.f());
var ghw = gw.makeDebuggeeValue(g.h);

// Specifying a line outside of all functions screens out all function scripts.
assertEq(dbg.findScripts({url:scriptname, line:3}).indexOf(gfw.script) != -1, false);
assertEq(dbg.findScripts({url:scriptname, line:3}).indexOf(ggw.script) != -1, false);
assertEq(dbg.findScripts({url:scriptname, line:3}).indexOf(ghw.script) != -1, false);

// Specifying a different url screens out scripts, even when global and line match.
assertEq(dbg.findScripts({url:"xlerb", line:8}).indexOf(gfw.script) != -1, false);
assertEq(dbg.findScripts({url:"xlerb", line:8}).indexOf(ggw.script) != -1, false);
assertEq(dbg.findScripts({url:"xlerb", line:8}).indexOf(ghw.script) != -1, false);

// A line number within a function selects that function's script.
assertEq(dbg.findScripts({url:scriptname, line:8}).indexOf(gfw.script) != -1, true);
assertEq(dbg.findScripts({url:scriptname, line:8}).indexOf(ggw.script) != -1, false);
assertEq(dbg.findScripts({url:scriptname, line:8}).indexOf(ghw.script) != -1, false);

// A line number within a nested function selects all enclosing functions' scripts.
assertEq(dbg.findScripts({url:scriptname, line:10}).indexOf(gfw.script) != -1, true);
assertEq(dbg.findScripts({url:scriptname, line:10}).indexOf(ggw.script) != -1, true);
assertEq(dbg.findScripts({url:scriptname, line:10}).indexOf(ghw.script) != -1, false);

// A line number in a non-nested function selects that function.
assertEq(dbg.findScripts({url:scriptname, line:15}).indexOf(gfw.script) != -1, false);
assertEq(dbg.findScripts({url:scriptname, line:15}).indexOf(ggw.script) != -1, false);
assertEq(dbg.findScripts({url:scriptname, line:15}).indexOf(ghw.script) != -1, true);
