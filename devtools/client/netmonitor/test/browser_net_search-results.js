/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test search match functionality.
 * Search panel is visible and clicking matches shows them in the request details.
 */

add_task(async function() {
  await pushPref("devtools.netmonitor.features.search", true);

  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  const SEARCH_STRING = "test";
  // Execute two XHRs and wait until they are finished.
  const URLS = [SEARCH_SJS + "?value=test1", SEARCH_SJS + "?value=test2"];
  const wait = waitForNetworkEvents(monitor, 2);
  for (const url of URLS) {
    await SpecialPowers.spawn(tab.linkedBrowser, [url], async function(
      requestUrl
    ) {
      content.wrappedJSObject.performRequests(1, requestUrl);
    });
  }
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
  type(SEARCH_STRING);
  EventUtils.synthesizeKey("KEY_Enter");

  // Wait till there are two resources rendered in the results.
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resourceRow",
    2
  );

  const searchMatchContents = document.querySelectorAll(
    ".search-panel-content .treeRow .treeIcon"
  );

  // Click both matches to expand them
  AccessibilityUtils.setEnv({
    // Keyboard users use arrow keys to expand/collapse tree items.
    // Accessibility is handled on the container level.
    mustHaveAccessibleRule: false,
  });
  for (let i = 0; i < searchMatchContents.length; i++) {
    EventUtils.sendMouseEvent({ type: "click" }, searchMatchContents[i]);
  }
  AccessibilityUtils.resetEnv();

  // Check the matches
  const matches = document.querySelectorAll(
    ".search-panel-content .treeRow.resultRow"
  );

  // Wait till there are two resources rendered in the results
  await waitForDOMIfNeeded(
    document,
    ".search-panel-content .treeRow.resourceRow",
    2
  );

  await checkSearchResult(
    monitor,
    matches[0],
    "#headers-panel",
    ".url-preview .properties-view",
    ".treeRow",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[1],
    "#headers-panel",
    "#responseHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[2],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[3],
    "#cookies-panel",
    "#responseCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[4],
    "#response-panel",
    ".CodeMirror-code",
    ".CodeMirror-activeline",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[5],
    "#headers-panel",
    ".url-preview .properties-view",
    ".treeRow",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[6],
    "#headers-panel",
    "#responseHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[7],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[8],
    "#headers-panel",
    "#requestHeaders .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[9],
    "#cookies-panel",
    "#requestCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[10],
    "#cookies-panel",
    "#responseCookies .properties-view",
    ".treeRow.selected",
    [SEARCH_STRING]
  );
  await checkSearchResult(
    monitor,
    matches[11],
    "#response-panel",
    ".CodeMirror-code",
    ".CodeMirror-activeline",
    [SEARCH_STRING]
  );

  await teardown(monitor);
});

/**
 * Check whether the search result is correctly linked with the related information
 */
async function checkSearchResult(
  monitor,
  match,
  panelSelector,
  panelContentSelector,
  panelDetailSelector,
  expected
) {
  const { document } = monitor.panelWin;

  // Click on the first match to show it
  EventUtils.sendMouseEvent({ type: "click" }, match);

  console.log(`${panelSelector} ${panelContentSelector}`);
  await waitFor(() =>
    document.querySelector(`${panelSelector} ${panelContentSelector}`)
  );

  const tabpanel = document.querySelector(panelSelector);
  const content = tabpanel.querySelectorAll(
    `${panelContentSelector} ${panelDetailSelector}`
  );

  is(
    content.length,
    expected.length,
    `There should be ${expected.length} item${
      expected.length === 1 ? "" : "s"
    } displayed in this tabpanel`
  );

  if (content.length === expected.length) {
    for (let i = 0; i < expected.length; i++) {
      is(
        content[i].textContent.includes(expected[i]),
        true,
        `Content must include ${expected[i]}`
      );
    }
  }
}
