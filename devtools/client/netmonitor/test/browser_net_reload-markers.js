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

  let markersDone = waitForTimelineMarkers(monitor);

  let button = document.querySelector(".requests-list-reload-notice-button");
  button.click();

  yield waitForNetworkEvents(monitor, 1);
  let markers = yield markersDone;

  ok(true, "Reloading finished");

  is(markers[0].name, "document::DOMContentLoaded",
    "The first received marker is correct.");
  is(markers[1].name, "document::Load",
    "The second received marker is correct.");

  return teardown(monitor);
});
