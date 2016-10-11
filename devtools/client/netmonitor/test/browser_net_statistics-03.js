/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the correct filtering predicates are used when filtering from
 * the performance analysis view.
 */

add_task(function* () {
  let { monitor } = yield initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  let panel = monitor.panelWin;
  let { $, EVENTS, NetMonitorView } = panel;

  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-ws-button"));
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-other-button"));
  testFilterButtonsCustom(monitor, [0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1]);
  info("The correct filtering predicates are used before entering perf. analysis mode.");

  let onEvents = promise.all([
    panel.once(EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
    panel.once(EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
  ]);
  NetMonitorView.toggleFrontendMode();
  yield onEvents;

  is(NetMonitorView.currentFrontendMode, "network-statistics-view",
    "The frontend mode is switched to the statistics view.");

  EventUtils.sendMouseEvent({ type: "click" }, $(".pie-chart-slice"));

  is(NetMonitorView.currentFrontendMode, "network-inspector-view",
    "The frontend mode is switched back to the inspector view.");

  testFilterButtons(monitor, "html");
  info("The correct filtering predicate is used when exiting perf. analysis mode.");

  yield teardown(monitor);
});
