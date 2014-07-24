// Owning elements and attribute names are attached to scripts compiled
// off-thread.

if (helperThreadCount() === 0)
  quit(0);

var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

var elt = new g.Object;
var eltDO = gDO.makeDebuggeeValue(elt);

var log = '';
dbg.onDebuggerStatement = function (frame) {
  log += 'd';
  var source = frame.script.source;
  assertEq(source.element, eltDO);
  assertEq(source.elementAttributeName, 'mass');
};

g.offThreadCompileScript('debugger;',
                         { element: elt,
                           elementAttributeName: 'mass' });
log += 'o';
g.runOffThreadScript();
assertEq(log, 'od');
