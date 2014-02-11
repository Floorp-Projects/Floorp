/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the correct filtering predicates are used when filtering from
 * the performance analysis view.
 */

function test() {
  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let panel = aMonitor.panelWin;
    let { $, EVENTS, NetMonitorView } = panel;

    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
    EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-other-button"));
    testFilterButtonsCustom(aMonitor, [0, 1, 1, 1, 0, 0, 0, 0, 0, 1]);
    ok(true, "The correct filtering predicates are used before entering perf. analysis mode.");

    promise.all([
      waitFor(panel, EVENTS.PRIMED_CACHE_CHART_DISPLAYED),
      waitFor(panel, EVENTS.EMPTY_CACHE_CHART_DISPLAYED)
    ]).then(() => {
      EventUtils.sendMouseEvent({ type: "click" }, $(".pie-chart-slice"));
      testFilterButtons(aMonitor, "html");
      ok(true, "The correct filtering predicate is used when exiting perf. analysis mode.");

      teardown(aMonitor).then(finish);
    });

    NetMonitorView.toggleFrontendMode();
  });
}
