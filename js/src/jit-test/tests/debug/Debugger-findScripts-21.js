// Test that delazification works after compartment merging.

if (helperThreadCount() === 0)
  quit(0);

var g = newGlobal();
var dbg = new Debugger(g);

var log;
dbg.onNewScript = function (s) {
  log += "s";
  log += dbg.findScripts({ source: s.source }).length;
}

// Delazify everything just in case before we start the off-thread compile.
dbg.findScripts();

log = "";
g.offThreadCompileScript("function inner() { function inner2() { print('inner2'); } print('inner'); }");
g.runOffThreadScript();
assertEq(log, "s3");
