/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network monitor panes collapse properly.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, windowRequire } = monitor.panelWin;
  let { Prefs } = windowRequire("devtools/client/netmonitor/prefs");
  let detailsPaneToggleButton = document.querySelector(".network-details-panel-toggle");

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  yield wait;

  ok(!document.querySelector(".network-details-panel") &&
     detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details panel should initially be hidden.");

  EventUtils.sendMouseEvent({ type: "click" }, detailsPaneToggleButton);

  is(~~(document.querySelector(".network-details-panel").clientWidth),
    Prefs.networkDetailsWidth,
    "The details panel has an incorrect width.");
  ok(document.querySelector(".network-details-panel") &&
     !detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details panel should at this point be visible.");

  EventUtils.sendMouseEvent({ type: "click" }, detailsPaneToggleButton);

  ok(!document.querySelector(".network-details-panel") &&
     detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details panel should not be visible after collapsing.");

  EventUtils.sendMouseEvent({ type: "click" }, detailsPaneToggleButton);

  is(~~(document.querySelector(".network-details-panel").clientWidth),
    Prefs.networkDetailsWidth,
    "The details panel has an incorrect width after uncollapsing.");
  ok(document.querySelector(".network-details-panel") &&
     !detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details panel should be visible again after uncollapsing.");

  yield teardown(monitor);
});
