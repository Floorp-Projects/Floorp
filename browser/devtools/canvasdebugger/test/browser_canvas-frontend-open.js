/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the frontend UI is properly configured when opening the tool.
 */

function* ifTestingSupported() {
  let { target, panel } = yield initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { $ } = panel.panelWin;

  is($("#snapshots-pane").hasAttribute("hidden"), false,
    "The snapshots pane should initially be visible.");
  is($("#debugging-pane").hasAttribute("hidden"), false,
    "The debugging pane should initially be visible.");

  is($("#record-snapshot").getAttribute("hidden"), "true",
    "The 'record snapshot' button should initially be hidden.");
  is($("#import-snapshot").hasAttribute("hidden"), false,
    "The 'import snapshot' button should initially be visible.");
  is($("#clear-snapshots").hasAttribute("hidden"), false,
    "The 'clear snapshots' button should initially be visible.");

  is($("#reload-notice").hasAttribute("hidden"), false,
    "The reload notice should initially be visible.");
  is($("#empty-notice").getAttribute("hidden"), "true",
    "The empty notice should initially be hidden.");
  is($("#waiting-notice").getAttribute("hidden"), "true",
    "The waiting notice should initially be hidden.");

  is($("#screenshot-container").getAttribute("hidden"), "true",
    "The screenshot container should initially be hidden.");
  is($("#snapshot-filmstrip").getAttribute("hidden"), "true",
    "The snapshot filmstrip should initially be hidden.");

  is($("#debugging-pane-contents").getAttribute("hidden"), "true",
    "The rest of the UI should initially be hidden.");

  yield teardown(panel);
  finish();
}
