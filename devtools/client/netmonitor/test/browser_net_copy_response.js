/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying a request's response works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  const EXPECTED_RESULT = '{ "greeting": "Hello JSON!" }';

  let { document } = monitor.panelWin;

  let wait = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[3]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[3]);

  yield waitForClipboardPromise(function setup() {
    // Context menu is appending in XUL document, we must select it from
    // _toolbox.doc
    monitor._toolbox.doc
      .querySelector("#request-menu-context-copy-response").click();
  }, EXPECTED_RESULT);

  yield teardown(monitor);
});
