/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's response works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  const EXPECTED_RESULT = '{ "greeting": "Hello JSON!" }';

  const { document } = monitor.panelWin;

  // Execute requests.
  await performRequests(monitor, tab, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[3]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[3]);

  await waitForClipboardPromise(function setup() {
    monitor.panelWin.parent.document
      .querySelector("#request-list-context-copy-response").click();
  }, EXPECTED_RESULT);

  await teardown(monitor);
});
