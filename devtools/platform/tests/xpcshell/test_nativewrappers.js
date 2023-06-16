"use strict";

function run_test() {
  const { addDebuggerToGlobal } = ChromeUtils.importESModule(
    "resource://gre/modules/jsdebugger.sys.mjs"
  );

  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });

  addDebuggerToGlobal(globalThis);
  const g = testGlobal("test1");

  const dbg = new Debugger();
  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function (frame) {
    const args = frame.arguments;
    try {
      args[0];
      Assert.ok(true);
    } catch (ex) {
      Assert.ok(false);
    }
  };

  g.eval("function stopMe(arg) {debugger;}");

  const g2 = testGlobal("test2");
  g2.g = g;
  g2.eval(
    "(" +
      function createBadEvent() {
        // eslint-disable-next-line mozilla/reject-importGlobalProperties
        Cu.importGlobalProperties(["DOMParser"]);
        const parser = new DOMParser();
        const doc = parser.parseFromString("<foo></foo>", "text/xml");
        g.stopMe(doc.createEvent("MouseEvent"));
      } +
      ")()"
  );

  dbg.removeAllDebuggees();
}
