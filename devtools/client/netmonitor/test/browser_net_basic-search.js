/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic search functionality.
 * Search panel is visible and number of expected results are returned.
 */

add_task(async function () {
  await pushPref("devtools.netmonitor.features.search", true);

  const { tab, monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute two XHRs (the same URL) and wait till it's finished.
  const URL = HTTPS_SEARCH_SJS + "?value=test";
  const wait = waitForNetworkEvents(monitor, 2);

  await SpecialPowers.spawn(tab.linkedBrowser, [URL], async function (url) {
    content.wrappedJSObject.performRequests(2, url);
  });
  await wait;

  // Open the Search panel
  store.dispatch(Actions.openSearch());

  // Fill Filter input with text and check displayed messages.
  // The filter should be focused automatically.
  typeInNetmonitor("test", monitor);
  EventUtils.synthesizeKey("KEY_Enter");

  // Wait till there are two resources rendered in the results.
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resourceRow",
    2
  );

  // Click on the first resource to expand it
  AccessibilityUtils.setEnv({
    // Keyboard users use arrow keys to expand/collapse tree items.
    // Accessibility is handled on the container level.
    mustHaveAccessibleRule: false,
  });
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector(".search-panel-content .treeRow .treeIcon")
  );
  AccessibilityUtils.resetEnv();

  // Check that there is 5 matches.
  const matches = document.querySelectorAll(
    ".search-panel-content .treeRow.resultRow"
  );
  is(matches.length, 5, "There must be 5 matches");

  await teardown(monitor);
});
