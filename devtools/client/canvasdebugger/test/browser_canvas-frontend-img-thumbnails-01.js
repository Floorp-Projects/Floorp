/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if thumbnails are properly displayed in the UI.
 */

async function ifTestingSupported() {
  const { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  const { window, $, $all, EVENTS, SnapshotsListView } = panel.panelWin;

  await reload(target);

  const recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  const callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  const thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  await Promise.all([recordingFinished, callListPopulated, thumbnailsDisplayed]);

  is($all(".filmstrip-thumbnail").length, 4,
    "There should be 4 thumbnails displayed in the UI.");

  const firstThumbnail = $(".filmstrip-thumbnail[index='0']");
  ok(firstThumbnail,
    "The first thumbnail element should be for the function call at index 0.");
  is(firstThumbnail.width, 50,
    "The first thumbnail's width is correct.");
  is(firstThumbnail.height, 50,
    "The first thumbnail's height is correct.");
  is(firstThumbnail.getAttribute("flipped"), "false",
    "The first thumbnail should not be flipped vertically.");

  const secondThumbnail = $(".filmstrip-thumbnail[index='2']");
  ok(secondThumbnail,
    "The second thumbnail element should be for the function call at index 2.");
  is(secondThumbnail.width, 50,
    "The second thumbnail's width is correct.");
  is(secondThumbnail.height, 50,
    "The second thumbnail's height is correct.");
  is(secondThumbnail.getAttribute("flipped"), "false",
    "The second thumbnail should not be flipped vertically.");

  const thirdThumbnail = $(".filmstrip-thumbnail[index='4']");
  ok(thirdThumbnail,
    "The third thumbnail element should be for the function call at index 4.");
  is(thirdThumbnail.width, 50,
    "The third thumbnail's width is correct.");
  is(thirdThumbnail.height, 50,
    "The third thumbnail's height is correct.");
  is(thirdThumbnail.getAttribute("flipped"), "false",
    "The third thumbnail should not be flipped vertically.");

  const fourthThumbnail = $(".filmstrip-thumbnail[index='6']");
  ok(fourthThumbnail,
    "The fourth thumbnail element should be for the function call at index 6.");
  is(fourthThumbnail.width, 50,
    "The fourth thumbnail's width is correct.");
  is(fourthThumbnail.height, 50,
    "The fourth thumbnail's height is correct.");
  is(fourthThumbnail.getAttribute("flipped"), "false",
    "The fourth thumbnail should not be flipped vertically.");

  await teardown(panel);
  finish();
}
