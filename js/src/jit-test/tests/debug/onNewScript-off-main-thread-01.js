// |jit-test| skip-if: helperThreadCount() === 0

// We still get onNewScript notifications for code compiled off the main thread.

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

var log;
dbg.onNewScript = function (s) {
  log += 's';
  assertEq(s.source.text, '"t" + "wine"');
}

log = '';
g.offThreadCompileScript('"t" + "wine"');
assertEq(g.runOffThreadScript(), 'twine');
assertEq(log, 's');
