/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test basic search functionality.
 * Search panel is visible and number of expected results are returned.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  // Execute two XHRs (the same URL) and wait till it's finished.
  const URL = SEARCH_SJS + "?value=test";
  const wait = waitForNetworkEvents(monitor, 2);

  await ContentTask.spawn(tab.linkedBrowser, URL, async function(url) {
    content.wrappedJSObject.performRequests(2, url);
  });
  await wait;

  // Open the Search panel
  store.dispatch(Actions.openSearch());

  // Helper for keyboard typing
  const type = string => {
    for (const ch of string) {
      EventUtils.synthesizeKey(ch, {}, monitor.panelWin);
    }
  };

  // Fill Filter input with text and check displayed messages.
  // The filter should be focused automatically.
  type("test");
  EventUtils.synthesizeKey("KEY_Enter");

  // Wait till there are two resources rendered in the results.
  await waitForDOM(document, ".search-panel-content .treeRow.resourceRow", 2);

  // Click on the first resource to expand it
  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector(".search-panel-content .treeRow .treeIcon")
  );

  // Check that there is 5 matches.
  const matches = document.querySelectorAll(
    ".search-panel-content .treeRow.resultRow"
  );
  is(matches.length, 5, "There must be 5 matches");

  await teardown(monitor);
});
