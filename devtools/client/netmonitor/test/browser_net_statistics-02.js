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
  let { document, gStore, windowRequire } = panel;
  let Actions = windowRequire("devtools/client/netmonitor/actions/index");

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-filter-html-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-filter-css-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-filter-js-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-filter-ws-button"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-filter-other-button"));
  testFilterButtonsCustom(monitor, [0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1]);
  info("The correct filtering predicates are used before entering perf. analysis mode.");

  gStore.dispatch(Actions.openStatistics(true));

  let body = document.querySelector("#body");

  is(body.selectedPanel.id, "statistics-panel",
    "The main panel is switched to the statistics panel.");

  yield waitUntil(
    () => document.querySelectorAll(".pie-chart-container:not([placeholder=true])").length == 2);
  ok(true, "Two real pie charts appear to be rendered correctly.");

  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".pie-chart-slice"));

  is(body.selectedPanel.id, "inspector-panel",
    "The main panel is switched back to the inspector panel.");

  testFilterButtons(monitor, "html");
  info("The correct filtering predicate is used when exiting perf. analysis mode.");

  yield teardown(monitor);
});
