/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if thumbnails are correctly linked with other UI elements like
 * function call items and their respective screenshots.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, $, $all, EVENTS, SnapshotsListView, CallsListView } = panel.panelWin;

  await reload(target);

  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  const callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  const thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
  const screenshotDisplayed = once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  await Promise.all([
    recordingFinished,
    callListPopulated,
    thumbnailsDisplayed,
    screenshotDisplayed
  ]);

  is($all(".filmstrip-thumbnail[highlighted]").length, 0,
    "There should be no highlighted thumbnail available yet.");
  is(CallsListView.selectedIndex, -1,
    "There should be no selected item in the calls list view.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, $all(".filmstrip-thumbnail")[0], window);
  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  info("The first draw call was selected, by clicking the first thumbnail.");

  isnot($(".filmstrip-thumbnail[highlighted][index='0']"), null,
    "There should be a highlighted thumbnail available now, for the first draw call.");
  is($all(".filmstrip-thumbnail[highlighted]").length, 1,
    "There should be only one highlighted thumbnail available now.");
  is(CallsListView.selectedIndex, 0,
    "The first draw call should be selected in the calls list view.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, $all(".call-item-view")[1], window);
  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  info("The second context call was selected, by clicking the second call item.");

  isnot($(".filmstrip-thumbnail[highlighted][index='0']"), null,
    "There should be a highlighted thumbnail available, for the first draw call.");
  is($all(".filmstrip-thumbnail[highlighted]").length, 1,
    "There should be only one highlighted thumbnail available.");
  is(CallsListView.selectedIndex, 1,
    "The second draw call should be selected in the calls list view.");

  EventUtils.sendMouseEvent({ type: "mousedown" }, $all(".call-item-view")[2], window);
  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  info("The second draw call was selected, by clicking the third call item.");

  isnot($(".filmstrip-thumbnail[highlighted][index='2']"), null,
    "There should be a highlighted thumbnail available, for the second draw call.");
  is($all(".filmstrip-thumbnail[highlighted]").length, 1,
    "There should be only one highlighted thumbnail available.");
  is(CallsListView.selectedIndex, 2,
    "The second draw call should be selected in the calls list view.");

  await teardown(panel);
  finish();
}
