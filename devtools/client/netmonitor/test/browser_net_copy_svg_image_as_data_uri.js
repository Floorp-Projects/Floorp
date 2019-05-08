/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if copying an image as data uri works.
 */

const SVG_URL = EXAMPLE_URL + "dropmarker.svg";

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CURL_URL);
  info("Starting test... ");

  const { document } = monitor.panelWin;

  const wait = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, SVG_URL, async function(url) {
    content.wrappedJSObject.performRequest(url);
  });
  await wait;

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]);
  EventUtils.sendMouseEvent({ type: "contextmenu" },
    document.querySelectorAll(".request-list-item")[0]);

  await waitForClipboardPromise(function setup() {
    monitor.panelWin.parent.document
      .querySelector("#request-list-context-copy-image-as-data-uri").click();
  }, function check(text) {
    return text.startsWith("data:") && !/undefined/.test(text);
  });

  await teardown(monitor);
});
