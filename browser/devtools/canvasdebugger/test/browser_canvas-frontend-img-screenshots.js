/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if screenshots are properly displayed in the UI.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, SnapshotsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  let screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated, screenshotDisplayed]);

  is($("#screenshot-container").hidden, false,
    "The screenshot container should now be visible.");

  is($("#screenshot-dimensions").getAttribute("value"), "128 x 128",
    "The screenshot dimensions label has the expected value.");

  is($("#screenshot-image").getAttribute("flipped"), "false",
    "The screenshot element should not be flipped vertically.");

  ok(window.getComputedStyle($("#screenshot-image")).backgroundImage.contains("#screenshot-rendering"),
    "The screenshot element should have an offscreen canvas element as a background.");

  yield teardown(panel);
  finish();
}
