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
  let { document, store, windowRequire, connector } = panel;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-html-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-css-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-js-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-ws-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".requests-list-filter-other-button"));
  testFilterButtonsCustom(monitor, [0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1]);
  info("The correct filtering predicates are used before entering perf. analysis mode.");

  store.dispatch(Actions.openStatistics(connector, true));

  ok(document.querySelector(".statistics-panel"),
    "The main panel is switched to the statistics panel.");

  yield waitUntil(
    () => document.querySelectorAll(".pie-chart-container:not([placeholder=true])")
                  .length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".pie-chart-slice"));

  ok(document.querySelector(".monitor-panel"),
    "The main panel is switched back to the monitor panel.");

  testFilterButtons(monitor, "html");
  info("The correct filtering predicate is used when exiting perf. analysis mode.");

  yield teardown(monitor);
});
