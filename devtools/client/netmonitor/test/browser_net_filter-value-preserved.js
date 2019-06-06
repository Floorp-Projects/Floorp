/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that filter input keeps its value when host or panel changes
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(FILTERING_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  const toolbars = document.querySelector("#netmonitor-toolbar-container");
  let input = toolbars.querySelector(".devtools-filterinput");
  input.value = "hello";

  await monitor.toolbox.switchHost("right");

  is(toolbars.querySelectorAll(".devtools-toolbar").length,
     2, "Should be in 2 toolbar mode");

  input = toolbars.querySelector(".devtools-filterinput");
  is(input.value, "hello", "Value should be preserved after switching to right host");

  await monitor.toolbox.switchHost("bottom");

  input = toolbars.querySelector(".devtools-filterinput");
  is(input.value, "hello", "Value should be preserved after switching to bottom host");

  await monitor.toolbox.selectTool("inspector");
  await monitor.toolbox.selectTool("netmonitor");

  input = toolbars.querySelector(".devtools-filterinput");
  is(input.value, "hello", "Value should be preserved after switching tools");

  await teardown(monitor);
});
