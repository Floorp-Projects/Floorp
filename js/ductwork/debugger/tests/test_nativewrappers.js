function run_test()
{
  const {addDebuggerToGlobal} = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
  const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });

  addDebuggerToGlobal(this);
  var g = testGlobal("test1");

  var dbg = new Debugger();
  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function(aFrame) {
    let args = aFrame["arguments"];
    try {
      args[0];
      Assert.ok(true);
    } catch(ex) {
      Assert.ok(false);
    }
  };

  g.eval("function stopMe(arg) {debugger;}");

  g2 = testGlobal("test2");
  g2.g = g;
  g2.eval("(" + function createBadEvent() {
    Cu.importGlobalProperties(["DOMParser"]);
    let parser = new DOMParser();
    let doc = parser.parseFromString("<foo></foo>", "text/xml");
    g.stopMe(doc.createEvent("MouseEvent"));
  } + ")()");

  dbg.removeAllDebuggees();
}
