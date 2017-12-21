/* eslint-disable strict */
function run_test() {
  Components.utils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(this);
  let xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);
  let g = testGlobal("test1");

  let dbg = new Debugger();
  dbg.uncaughtExceptionHook = testExceptionHook;

  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function (frame) {
    Assert.ok(frame === dbg.getNewestFrame());
    // Execute from the nested event loop, dbg.getNewestFrame() won't
    // be working anymore.

    do_execute_soon(function () {
      try {
        Assert.ok(frame === dbg.getNewestFrame());
      } finally {
        xpcInspector.exitNestedEventLoop("test");
      }
    });
    xpcInspector.enterNestedEventLoop("test");
  };

  g.eval("function debuggerStatement() { debugger; }; debuggerStatement();");

  dbg.enabled = false;
}
