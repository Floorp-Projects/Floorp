/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the slider in the calls list view works as advertised.
 */

function ifTestingSupported() {
  let [target, debuggee, panel] = yield initCanavsDebuggerFrontend(SIMPLE_CANVAS_URL);
  let { window, $, EVENTS, gFront, SnapshotsListView, CallsListView } = panel.panelWin;

  yield reload(target);

  let recordingFinished = once(window, EVENTS.SNAPSHOT_RECORDING_FINISHED);
  let callListPopulated = once(window, EVENTS.CALL_LIST_POPULATED);
  let thumbnailsDisplayed = once(window, EVENTS.THUMBNAILS_DISPLAYED);
  SnapshotsListView._onRecordButtonClick();
  yield promise.all([recordingFinished, callListPopulated, thumbnailsDisplayed]);

  let firstSnapshot = SnapshotsListView.getItemAtIndex(0);
  let firstSnapshotOverview = yield firstSnapshot.attachment.actor.getOverview();

  let thumbnails = firstSnapshotOverview.thumbnails;
  is(thumbnails.length, 4,
    "There should be 4 thumbnails cached for the snapshot item.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 1;
  let thumbnailPixels = yield thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[0].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  yield once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 1.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 2;
  let thumbnailPixels = yield thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[1].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  yield once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 2.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 7;
  let thumbnailPixels = yield thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[3].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  yield once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 7.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 4;
  let thumbnailPixels = yield thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[2].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  yield once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 4.");

  let thumbnailImageElementSet = waitForMozSetImageElement(window);
  $("#calls-slider").value = 0;
  let thumbnailPixels = yield thumbnailImageElementSet;

  ok(sameArray(thumbnailPixels, thumbnails[0].pixels),
    "The screenshot element should have a thumbnail as an immediate background.");

  yield once(window, EVENTS.CALL_SCREENSHOT_DISPLAYED);
  ok(true, "The full-sized screenshot was displayed for the item at index 0.");

  yield teardown(panel);
  finish();
}

function waitForMozSetImageElement(panel) {
  let deferred = promise.defer();
  panel._onMozSetImageElement = deferred.resolve;
  return deferred.promise;
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
