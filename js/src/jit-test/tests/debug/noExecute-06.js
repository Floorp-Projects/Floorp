// Tests that NX disallows debuggee execution for multiple debuggers and
// multiple debuggees.

load(libdir + "asserts.js");
load(libdir + "debuggerNXHelper.js");

var g1 = newGlobal();
var g2 = newGlobal();
var dbg1 = new Debugger;
var dbg2 = new Debugger;

g1w1 = dbg1.addDebuggee(g1);

g1w2 = dbg2.addDebuggee(g1);
g2w = dbg2.addDebuggee(g2);

g1.eval(`
        function d(f) { debugger; return f; }
        function f() { return 42; }
        var o = {
          get p() { return 42; },
          set p(x) { }
        };
        `);

g2.eval(`
        function d(f) { debugger; return f; }
        function f() { return 42; }
        var o = {
          get p() { return 42; },
          set p(x) { }
        };
        `);

var strs = ["f();", "o.p", "o.p = 42"];

var fw1;
dbg1.onDebuggerStatement = (frame) => {
  fw1 = frame.arguments[0];
}
g1.eval('d(f)');
dbg1.onDebuggerStatement = undefined;
var fw2;
dbg2.onDebuggerStatement = (frame) => {
  fw2 = frame.arguments[0];
}
g2.eval('d(f)');
dbg2.onDebuggerStatement = undefined;

function testHook(hookName) {
  var newestG1Frame = dbg1.getNewestFrame();
  if (hookName != 'onNewGlobalObject' &&
      hookName != 'onNewScript' &&
      hookName != 'onNewPromise' &&
      hookName != 'onPromiseSettled')
  {
    var newestG2Frame = dbg2.getNewestFrame();
  }

  for (var s of strs) {
    // When this hook is called, g1 has been locked twice, so even invocation
    // functions do not work.
    assertEq(g1w1.executeInGlobal(s).throw.unsafeDereference() instanceof Debugger.DebuggeeWouldRun, true);
    assertEq(g1w2.executeInGlobal(s).throw.unsafeDereference() instanceof Debugger.DebuggeeWouldRun, true);
    if (newestG1Frame) {
      assertEq(newestG1Frame.eval(s).throw.unsafeDereference() instanceof Debugger.DebuggeeWouldRun, true);
    }
    assertEq(fw1.apply(null).throw.unsafeDereference() instanceof Debugger.DebuggeeWouldRun, true);

    // But g2 has only been locked once and so should work.
    assertEq(g2w.executeInGlobal(s).throw, undefined);
    if (newestG2Frame) {
      assertEq(newestG2Frame.eval(s).throw, undefined);
    }
    assertEq(fw2.apply(null).return, 42);
  }
}

testDebuggerHooksNX(dbg1, g1, () => {
  testDebuggerHooksNX(dbg2, g2, testHook);
});
