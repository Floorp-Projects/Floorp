/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

const SVG_URL = EXAMPLE_URL + "dropmarker.svg";

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CURL_URL);
  info("Starting test... ");

  let { document } = monitor.panelWin;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, SVG_URL, function* (url) {
    content.wrappedJSObject.performRequest(url);
  });
  yield wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]);

  yield waitForClipboardPromise(function setup() {
    monitor.panelWin.parent.document
      .querySelector("#request-list-context-copy-image-as-data-uri").click();
  }, function check(text) {
    return text.startsWith("data:") && !/undefined/.test(text);
  });

  yield teardown(monitor);
});
