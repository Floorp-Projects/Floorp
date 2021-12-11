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
  const g = testGlobal("test1");

  const dbg = makeDebugger();
  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function(frame) {
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
  // Not using the "stringify a function" trick because that runs afoul of the
  // Cu.importGlobalProperties lint and we don't need it here anyway.
  g2.eval(`(function createBadEvent() {
    Cu.importGlobalProperties(["DOMParser"]);
    let parser = new DOMParser();
    let doc = parser.parseFromString("<foo></foo>", "text/xml");
    g.stopMe(doc.createEvent("MouseEvent"));
  } )()`);

  dbg.disable();
}
