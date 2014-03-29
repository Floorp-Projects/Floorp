/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if thumbnails are properly displayed in the UI.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, $all, EVENTS, SnapshotsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  let thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated, thumbnailsDisplayed]);

  is($all(".filmstrip-thumbnail").length, 4,
    "There should be 4 thumbnails displayed in the UI.");

  let firstThumbnail = $(".filmstrip-thumbnail[index='0']");
  ok(firstThumbnail,
    "The first thumbnail element should be for the function call at index 0.");
  is(firstThumbnail.width, 50,
    "The first thumbnail's width is correct.");
  is(firstThumbnail.height, 50,
    "The first thumbnail's height is correct.");
  is(firstThumbnail.getAttribute("flipped"), "false",
    "The first thumbnail should not be flipped vertically.");

  let secondThumbnail = $(".filmstrip-thumbnail[index='2']");
  ok(secondThumbnail,
    "The second thumbnail element should be for the function call at index 2.");
  is(secondThumbnail.width, 50,
    "The second thumbnail's width is correct.");
  is(secondThumbnail.height, 50,
    "The second thumbnail's height is correct.");
  is(secondThumbnail.getAttribute("flipped"), "false",
    "The second thumbnail should not be flipped vertically.");

  let thirdThumbnail = $(".filmstrip-thumbnail[index='4']");
  ok(thirdThumbnail,
    "The third thumbnail element should be for the function call at index 4.");
  is(thirdThumbnail.width, 50,
    "The third thumbnail's width is correct.");
  is(thirdThumbnail.height, 50,
    "The third thumbnail's height is correct.");
  is(thirdThumbnail.getAttribute("flipped"), "false",
    "The third thumbnail should not be flipped vertically.");

  let fourthThumbnail = $(".filmstrip-thumbnail[index='6']");
  ok(fourthThumbnail,
    "The fourth thumbnail element should be for the function call at index 6.");
  is(fourthThumbnail.width, 50,
    "The fourth thumbnail's width is correct.");
  is(fourthThumbnail.height, 50,
    "The fourth thumbnail's height is correct.");
  is(fourthThumbnail.getAttribute("flipped"), "false",
    "The fourth thumbnail should not be flipped vertically.");

  yield teardown(panel);
  finish();
}
