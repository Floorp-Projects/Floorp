/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// We make sure the menuitems in the application menubar
// are checked.

function test() {
  var tab1 = addTab("about:blank", function() {
    var tab2 = addTab("about:blank", function() {
      gBrowser.selectedTab = tab2;

      let pane = DebuggerUI.toggleDebugger();
      ok(pane, "toggleDebugger() should return a pane.");
      let frame = pane._frame;

      wait_for_connect_and_resume(function() {
        let cmd = document.getElementById("Tools:Debugger");
        is(cmd.getAttribute("checked"), "true", "<command Tools:Debugger> is checked.");

        gBrowser.selectedTab = tab1;

        is(cmd.getAttribute("checked"), "false", "<command Tools:Debugger> is unchecked after tab switch.");

        gBrowser.selectedTab = tab2;

        is(cmd.getAttribute("checked"), "true", "<command Tools:Debugger> is checked.");

        let pane = DebuggerUI.toggleDebugger();

        is(cmd.getAttribute("checked"), "false", "<command Tools:Debugger> is unchecked once closed.");
      });

      window.addEventListener("Debugger:Shutdown", function dbgShutdown() {
        window.removeEventListener("Debugger:Shutdown", dbgShutdown, true);
          removeTab(tab1);
          removeTab(tab2);

          finish();
      }, true);
    });
  });
}
