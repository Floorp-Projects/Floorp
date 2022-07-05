// Accessing the variables in the snapshot after popping the module
// should work until the module's top-level script's evaluation finishes.

var g = newGlobal({newCompartment: true});
g.eval(`
var accessVarInSnapshot = null;
`);
g.eval(`
var resolve;
var p = new Promise(r => resolve = r);
`);
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);
dbg.onDebuggerStatement = function (frame) {
  assertEq(frame.environment.getVariable("x"), 10);
  frame.eval("accessVarInSnapshot = function() { return ++x }");
  assertEq(gw.executeInGlobal("accessVarInSnapshot()").return, 11);
};
const m = g.parseModule(`
  let x = 10;
  await 20;
  debugger;
  await p;
`);
moduleLink(m);

// Run until `await p`.
moduleEvaluate(m);

drainJobQueue();

assertEq(g.accessVarInSnapshot !== null, true);

// At this point, the module's top-level script's evaluation isn't yet finished,
// and the module's ScriptSlot is still alive.
// Accessing the snapshot should work.
assertEq(g.accessVarInSnapshot(), 12);

// Continue the module's top-level script's evaluation.
g.eval(`
resolve();
`);

drainJobQueue();

// At this point, the module's ScriptSlot is cleared and accessing the
// snapshot doesn't work.
try {
  g.accessVarInSnapshot();
  // the above should throw.
  assertEq(true, false);
} catch (e) {
  assertEq(e+"", "ReferenceError: x is not defined");
}
