/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the empty-requests reload button works.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document } = monitor.panelWin;

  let wait = waitForNetworkEvents(monitor, 1);
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-reload-notice-button"));
  yield wait;

  is(document.querySelectorAll(".request-list-item").length, 1,
    "The request list should have one item after reloading");

  return teardown(monitor);
});
