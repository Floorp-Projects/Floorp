/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that network logs created when the Net panel is not visible
 * are displayed when the user shows the panel again.
 */
add_task(async () => {
  const { tab, monitor, toolbox } = await initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute two requests
  await performRequests(monitor, tab, 2);

  // Wait for two logs
  await waitUntil(() => document.querySelectorAll(".request-list-item").length == 2);

  info("Select the inspector");
  await toolbox.selectTool("inspector");

  info("Wait for Net panel to be hidden");
  await waitUntil(() => (document.visibilityState == "hidden"));

  // Execute another two requests
  await performRequests(monitor, tab, 2);

  // The number of rendered requests should be the same since
  // requests shouldn't be rendered while the net panel is in
  // background
  is(document.querySelectorAll(".request-list-item").length, 2,
    "There should be expected number of requests");

  info("Select the Net panel again");
  await toolbox.selectTool("netmonitor");

  // Wait for another two logs to be rendered since the panel
  // is selected now.
  await waitUntil(() => document.querySelectorAll(".request-list-item").length == 4);

  return teardown(monitor);
});
