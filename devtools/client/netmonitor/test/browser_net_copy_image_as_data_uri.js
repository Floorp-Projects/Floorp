/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 7);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  let requestItem = RequestsMenu.getItemAtIndex(5);
  RequestsMenu.selectedItem = requestItem;

  yield waitForClipboardPromise(function setup() {
    RequestsMenu.copyImageAsDataUri();
  }, TEST_IMAGE_DATA_URI);

  ok(true, "Clipboard contains the currently selected image as data uri.");

  yield teardown(monitor);
});
