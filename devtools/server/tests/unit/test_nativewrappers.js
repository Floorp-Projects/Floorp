/* eslint-disable strict */
function run_test() {
  Components.utils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(this);
  let g = testGlobal("test1");

  let dbg = new Debugger();
  dbg.addDebuggee(g);
  dbg.onDebuggerStatement = function (frame) {
    let args = frame.arguments;
    try {
      args[0];
      Assert.ok(true);
    } catch (ex) {
      Assert.ok(false);
    }
  };

  g.eval("function stopMe(arg) {debugger;}");

  g2 = testGlobal("test2");
  g2.g = g;
  g2.eval("(" + function createBadEvent() {
    let parser = Components.classes["@mozilla.org/xmlextras/domparser;1"]
        .createInstance(Components.interfaces.nsIDOMParser);
    let doc = parser.parseFromString("<foo></foo>", "text/xml");
    g.stopMe(doc.createEvent("MouseEvent"));
  } + ")()");

  dbg.enabled = false;
}
