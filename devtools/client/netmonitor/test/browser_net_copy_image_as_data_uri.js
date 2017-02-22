/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;

  let wait = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[5]);

  yield waitForClipboardPromise(function setup() {
    // Context menu is appending in XUL document, we must select it from
    // toolbox.doc
    monitor.toolbox.doc
      .querySelector("#request-list-context-copy-image-as-data-uri").click();
 }, TEST_IMAGE_DATA_URI);

  ok(true, "Clipboard contains the currently selected image as data uri.");

  yield teardown(monitor);
});
