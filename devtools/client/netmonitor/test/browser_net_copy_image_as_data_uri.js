/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  const { document } = monitor.panelWin;

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[5]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[5]);

  await waitForClipboardPromise(function setup() {
    getContextMenuItem(monitor, "request-list-context-copy-image-as-data-uri").click();
  }, TEST_IMAGE_DATA_URI);

  ok(true, "Clipboard contains the currently selected image as data uri.");

  await teardown(monitor);
});
