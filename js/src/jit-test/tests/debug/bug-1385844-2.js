// Frame invalidatation should not follow 'debugger eval prev' links.
// This should not fail a 'frame.isDebuggee()' assertion.
// This version of the test requires only a single Debugger, and a single
// debuggee global.

var g = newGlobal({ newCompartment: true });
var dbg = new Debugger(g);

g.eval(`
  function f() { debugger; }
`);
g.observeAll = observeAll;

dbg.onDebuggerStatement = first;
g.eval(`debugger;`);

var saved;
function first(frame) {
  saved = frame;
  dbg.onDebuggerStatement = second;
  saved.eval(`f()`);
}

function second() {
  saved.eval(`observeAll()`);
}

function observeAll() {
  // Setting this hook causes `Debugger::updateExecutionObservabilityOfFrames`
  // to walk the stack looking for frames running in `g` and marking them as
  // debuggees. It should ignore 'debugger eval prev' links; if it does not, the
  // traversal will jump from the frame for `second`'s eval directly to `saved`,
  // the frame for the `g.eval` call. In particular, it will not visit the frame
  // for the eval in `first`, which was never marked as a debuggee. (Simply
  // being created for a `Debugger.Frame.prototype.eval` call doesn't
  // necessarily mark you as a debuggee, if your behavior doesn't need to be
  // observed.)
  dbg.onEnterFrame = function () { };
}
