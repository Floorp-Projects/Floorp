/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var tab1 = addTab(TAB1_URL, function() {
    gBrowser.selectedTab = tab1;

    ok(!DebuggerUI.getDebugger(gBrowser.selectedTab),
      "Shouldn't have a debugger pane for this tab yet.");

    let pane = DebuggerUI.toggleDebugger();
    let someHeight = parseInt(Math.random() * 200) + 200;

    ok(pane, "toggleDebugger() should return a pane.");

    is(DebuggerUI.getDebugger(gBrowser.selectedTab), pane,
      "getDebugger() should return the same pane as toggleDebugger().");

    ok(DebuggerUI.preferences.height,
      "The debugger preferences should have a saved height value.");

    is(DebuggerUI.preferences.height, pane.frame.height,
      "The debugger pane height should be the same as the preferred value.");

    pane.frame.height = someHeight;
    ok(DebuggerUI.preferences.height !== someHeight,
      "Height preferences shouldn't have been updated yet.");

    pane.onConnected = function() {
      removeTab(tab1);
      finish();

      is(DebuggerUI.preferences.height, someHeight,
        "Height preferences should have been updated by now.");
    };
  });
}
