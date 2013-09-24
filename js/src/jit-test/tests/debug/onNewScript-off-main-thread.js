// We still get onNewScript notifications for code compiled off the main thread.

if (!getBuildConfiguration().threadsafe)
  quit(0);

var g = newGlobal();
var dbg = new Debugger(g);

var log;
dbg.onNewScript = function (s) {
  log += 's';
  assertEq(s.source.text, '"t" + "wine"');
}

log = '';
g.offThreadCompileScript('"t" + "wine"');
assertEq(runOffThreadScript(), 'twine');
assertEq(log, 's');
