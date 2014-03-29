/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the frontend UI is properly reconfigured after reloading.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS } = panel.panelWin;

  let reset = once(window, EVENTS.UI_RESET);
  let navigated = reload(target);

  yield reset;
  ok(true, "The UI was reset after the refresh button was clicked.");

  yield navigated;
  ok(true, "The target finished reloading.");

  is($("#snapshots-pane").hasAttribute("hidden"), false,
    "The snapshots pane should still be visible.");
  is($("#debugging-pane").hasAttribute("hidden"), false,
    "The debugging pane should still be visible.");

  is($("#record-snapshot").hasAttribute("checked"), false,
    "The 'record snapshot' button should not be checked.");
  is($("#record-snapshot").hasAttribute("disabled"), false,
    "The 'record snapshot' button should not be disabled.");

  is($("#record-snapshot").hasAttribute("hidden"), false,
    "The 'record snapshot' button should now be visible.");
  is($("#import-snapshot").hasAttribute("hidden"), false,
    "The 'import snapshot' button should still be visible.");
  is($("#clear-snapshots").hasAttribute("hidden"), false,
    "The 'clear snapshots' button should still be visible.");

  is($("#reload-notice").getAttribute("hidden"), "true",
    "The reload notice should now be hidden.");
  is($("#empty-notice").hasAttribute("hidden"), false,
    "The empty notice should now be visible.");
  is($("#import-notice").getAttribute("hidden"), "true",
    "The import notice should now be hidden.");

  is($("#snapshot-filmstrip").getAttribute("hidden"), "true",
    "The snapshot filmstrip should still be hidden.");
  is($("#screenshot-container").getAttribute("hidden"), "true",
    "The screenshot container should still be hidden.");

  is($("#debugging-pane-contents").getAttribute("hidden"), "true",
    "The rest of the UI should still be hidden.");

  yield teardown(panel);
  finish();
}
