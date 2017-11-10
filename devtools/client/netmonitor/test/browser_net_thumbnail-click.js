/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clicking on the file thumbnail opens the response details tab.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  let { document } = monitor.panelWin;

  yield performRequestsAndWait();

  let wait = waitForDOM(document, "#response-panel");

  let request = document.querySelectorAll(".request-list-item")[5];
  let icon = request.querySelector(".requests-list-icon");

  info("Clicking thumbnail of the sixth request.");
  EventUtils.synthesizeMouseAtCenter(icon, {}, monitor.panelWin);

  yield wait;

  ok(document.querySelector("#response-tab[aria-selected=true]"),
     "Response tab is selected.");
  ok(document.querySelector(".response-image-box"),
     "Response image preview is shown.");

  yield teardown(monitor);

  function* performRequestsAndWait() {
    let onAllEvents = waitForNetworkEvents(monitor, CONTENT_TYPE_WITHOUT_CACHE_REQUESTS);
    yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.wrappedJSObject.performRequests();
    });
    yield onAllEvents;
  }
});
