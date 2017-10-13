/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the circular buffer notices work when e10s is on/off.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const { initPerformanceInNewTab, teardownToolboxAndRemoveTab } = require("devtools/client/performance/test/helpers/panel-utils");

add_task(function* () {
  let { panel } = yield initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window
  });

  let { gFront, $, PerformanceController } = panel.panelWin;

  // Set a fast profiler-status update interval
  yield gFront.setProfilerStatusInterval(10);

  let supported = false;
  let enabled = false;

  PerformanceController.getMultiprocessStatus = () => {
    return { supported, enabled };
  };

  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "unsupported",
    "When e10s is disabled and no option to turn on, container has [e10s=unsupported].");

  supported = true;
  enabled = false;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "disabled",
    "When e10s is disabled and but is supported, container has [e10s=disabled].");

  supported = false;
  enabled = true;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "",
    "When e10s is enabled, but not supported, this probably means we no longer have " +
    "E10S_TESTING_ONLY, and we have no e10s attribute.");

  supported = true;
  enabled = true;
  PerformanceController._setMultiprocessAttributes();
  ok($("#performance-view").getAttribute("e10s"), "",
    "When e10s is enabled and supported, there should be no e10s attribute.");

  yield teardownToolboxAndRemoveTab(panel);
});
