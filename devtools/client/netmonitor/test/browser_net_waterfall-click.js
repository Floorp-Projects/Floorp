/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clicking on the waterfall opens the timing sidebar panel.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  let { document } = monitor.panelWin;

  let onAllEvents = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.performRequests();
  });
  yield onAllEvents;

  info("Clicking waterfall and waiting for panel update.");
  let wait = waitForDOM(document, "#timings-panel");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelectorAll(".requests-list-timings")[0]);

  yield wait;

  ok(document.querySelector("#timings-tab[aria-selected=true]"),
     "Timings tab is selected.");

  return teardown(monitor);
});
