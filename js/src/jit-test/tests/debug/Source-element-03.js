// |jit-test| skip-if: helperThreadCount() === 0

// Owning elements and attribute names are attached to scripts compiled
// off-thread.

var g = newGlobal({ newCompartment: true });
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

var job = g.offThreadCompileScript('debugger;');
log += 'o';
g.runOffThreadScript(job,
  {
    element: elt,
    elementAttributeName: 'mass'
  });
assertEq(log, 'od');
