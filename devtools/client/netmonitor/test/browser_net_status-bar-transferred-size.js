"use strict";

/**
 * Test if the value of total data transferred is displayed correctly in the Status Bar
 * Test for Bug 1481002
 */
add_task(async () => {
  // Clear cache, so we see expected number of cached requests.
  Services.cache2.clear();
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);

  const {
    getFormattedSize,
  } = require("devtools/client/netmonitor/src/utils/format-utils");

  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, true);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequestsSummary } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  store.dispatch(Actions.batchEnable(false));

  info("Performing requests...");
  await performRequestsAndWait();

  let cachedItemsInUI = 0;
  for (const requestItem of document.querySelectorAll(".request-list-item")) {
    const requestTransferStatus = requestItem.querySelector(
      ".requests-list-transferred"
    ).textContent;
    if (requestTransferStatus === "cached") {
      cachedItemsInUI++;
    }
  }

  is(cachedItemsInUI, 1, "Number of cached requests displayed is correct");

  const state = store.getState();
  const totalRequestsCount = state.requests.requests.size;
  const requestsSummary = getDisplayedRequestsSummary(state);
  info(`Current requests: ${requestsSummary.count} of ${totalRequestsCount}.`);

  const valueTransfer = document.querySelector(
    ".requests-list-network-summary-transfer"
  ).textContent;
  info("Current summary transfer: " + valueTransfer);
  const expectedTransfer = L10N.getFormatStrWithNumbers(
    "networkMenu.summary.transferred",
    getFormattedSize(requestsSummary.contentSize),
    getFormattedSize(requestsSummary.transferredSize)
  );

  is(
    valueTransfer,
    expectedTransfer,
    "The current summary transfer is correct."
  );

  await teardown(monitor);

  async function performRequestsAndWait() {
    const wait = waitForNetworkEvents(monitor, 2);
    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
      content.wrappedJSObject.performOneCachedRequest();
    });
    await wait;
  }
});
