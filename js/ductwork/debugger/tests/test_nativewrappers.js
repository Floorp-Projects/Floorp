function run_test()
{
  ChromeUtils.import("resource://gre/modules/jsdebugger.jsm");
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

  dbg.enabled = false;
}
