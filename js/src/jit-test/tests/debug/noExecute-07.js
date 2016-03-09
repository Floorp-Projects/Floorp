// Tests provenance of Debugger.DebuggeeWouldRun errors.

load(libdir + "asserts.js");
load(libdir + "debuggerNXHelper.js");

var g1 = newGlobal();
var g2 = newGlobal();
var g3 = newGlobal();
var dbg = new Debugger(g1);

g3.eval(`var dbg = new Debugger`);
var g1w = g3.dbg.addDebuggee(g1);
g3.dbg.addDebuggee(g2);

g1.eval(`function f() {}`);

function testHook(hookName) {
  // The stack is like so:
  //   g1 -> dbg (locks g1) -> g2 -> g3.dbg (locks g1 and g2)
  //
  // The DebuggeeWouldRun error is always allocated in the topmost locked
  // Debugger's compartment.

  // If we try to run script in g1 without going through one of g3.dbg's
  // invocation functions, we should get an error allocated in
  // g3.Debugger.DebuggeeWouldRun.
  assertThrowsInstanceOf(() => { g1.eval(`f()`); }, g3.Debugger.DebuggeeWouldRun);

  // If we try to run script in g1 via one of g3.dbg's invocation functions,
  // we should get an error allocated in Debugger.DebuggeeWouldRun.
  assertEq(g1w.executeInGlobal(`f()`).throw.unsafeDereference() instanceof Debugger.DebuggeeWouldRun, true);
}

testDebuggerHooksNX(dbg, g1, () => {
  testDebuggerHooksNX(g3.dbg, g2, testHook);
});
