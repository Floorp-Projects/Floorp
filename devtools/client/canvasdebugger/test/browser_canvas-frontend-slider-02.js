/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the slider in the calls list view works as advertised.
 */

async function ifTestingSupported() {
  let { target, panel } = await initCanvasDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, gFront, SnapshotsListView, CallsListView } = panel.panelWin;

  await reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  let thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  await Promise.all([recordingFinished, callListPopulated, thumbnailsDisplayed]);

  let firstSnapshot = SnapshotsListView.getItemAtIndex(0);
  let firstSnapshotOverview = await firstSnapshot.attachment.actor.getOverview();

  let thumbnails = firstSnapshotOverview.thumbnails;
  is(thumbnails.length, 4,
    "There should be 4 thumbnails cached for the snapshot item.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 1;
  let thumbnailPixels = await thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[0].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 1.");

  thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 2;
  thumbnailPixels = await thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[1].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 2.");

  thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 7;
  thumbnailPixels = await thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[3].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 7.");

  thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 4;
  thumbnailPixels = await thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[2].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 4.");

  thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 0;
  thumbnailPixels = await thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[0].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  await once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 0.");

  await teardown(panel);
  finish();
}

function waitForMozSetImageElement(panel) {
  return new Promise((resolve, reject) => {
    panel._onMozSetImageElement = resolve;
  });
}

function sameArray(a, b) {
  if (a.length != b.length) {
    return false;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      return false;
    }
  }
  return true;
}
