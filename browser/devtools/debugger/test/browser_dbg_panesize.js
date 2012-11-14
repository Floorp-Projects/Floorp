/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var tab1 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = tab1;

    ok(!DebuggerUI.getDebugger(),
      "Shouldn't have a debugger pane for this tab yet.");

    let pane = DebuggerUI.toggleDebugger();
    ok(pane, "toggleDebugger() should return a pane.");

    let preferredHeight = Services.prefs.getIntPref("devtools.debugger.ui.height");
    let someHeight;

    do {
      someHeight = parseInt(Math.random() * 200) + 200;
    } while (someHeight == preferredHeight)

    info("Preferred pane height: " + preferredHeight);
    info("Generated pane height: " + someHeight);

    is(DebuggerUI.getDebugger(), pane,
      "getDebugger() should return the same pane as toggleDebugger().");

    ok(DebuggerUI.preferences.height,
      "The debugger preferences should have a saved height value.");

    is(DebuggerUI.preferences.height, pane._frame.height,
      "The debugger pane height should be the same as the preferred value.");

    pane._frame.height = someHeight;

    ok(DebuggerUI.preferences.height !== someHeight,
      "Height preferences shouldn't have been updated yet.");

    wait_for_connect_and_resume(function() {
      removeTab(tab1);
    });

    window.addEventListener("Debugger:Shutdown", function dbgShutdown() {
      window.removeEventListener("Debugger:Shutdown", dbgShutdown, true);

      is(DebuggerUI.preferences.height, someHeight,
        "Height preferences should have been updated by now.");

      finish();

    }, true);
  });
}
