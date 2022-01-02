/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that the circular buffer notices work when e10s is on/off.
 */

const { SIMPLE_URL } = require("devtools/client/performance/test/helpers/urls");
const {
  initPerformanceInNewTab,
  teardownToolboxAndRemoveTab,
} = require("devtools/client/performance/test/helpers/panel-utils");

add_task(async function() {
  const { panel } = await initPerformanceInNewTab({
    url: SIMPLE_URL,
    win: window,
  });

  const { $, PerformanceController } = panel.panelWin;

  // Set a fast profiler-status update interval
  const performanceFront = await panel.target.getFront("performance");
  await performanceFront.setProfilerStatusInterval(10);

  let enabled = false;

  PerformanceController.getMultiprocessStatus = () => {
    return { enabled };
  };

  PerformanceController._setMultiprocessAttributes();
  is(
    $("#performance-view").getAttribute("e10s"),
    "disabled",
    "When e10s is disabled, container has [e10s=disabled]."
  );

  enabled = true;

  PerformanceController._setMultiprocessAttributes();

  // XXX: Switched to from ok() to todo_is() in Bug 1467712. Follow up in 1500913
  // This cannot work with the current implementation, _setMultiprocessAttributes is not
  // removing existing attributes.
  todo_is(
    $("#performance-view").getAttribute("e10s"),
    "",
    "When e10s is enabled, there should be no e10s attribute."
  );

  await teardownToolboxAndRemoveTab(panel);
});
