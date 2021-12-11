/* eslint-disable strict */
function run_test() {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
  const { addDebuggerToGlobal } = ChromeUtils.import(
    "resource://gre/modules/jsdebugger.jsm"
  );
  addDebuggerToGlobal(this);
  const xpcInspector = Cc["@mozilla.org/jsinspector;1"].getService(
    Ci.nsIJSInspector
  );
  const g = testGlobal("test1");

  const dbg = makeDebugger();
  dbg.uncaughtExceptionHook = testExceptionHook;

  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function(frame) {
    Assert.ok(frame === dbg.getNewestFrame());
    // Execute from the nested event loop, dbg.getNewestFrame() won't
    // be working anymore.

    executeSoon(function() {
      try {
        Assert.ok(frame === dbg.getNewestFrame());
      } finally {
        xpcInspector.exitNestedEventLoop("test");
      }
    });
    xpcInspector.enterNestedEventLoop("test");
  };

  g.eval("function debuggerStatement() { debugger; }; debuggerStatement();");

  dbg.disable();
}
