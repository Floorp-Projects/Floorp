"use strict";

/**
 * Test if the value of total data transferred is displayed correctly in the Status Bar
 * Test for Bug 1481002
 */
add_task(async function testTotalTransferredSize() {
  // Clear cache, so we see expected number of cached requests.
  Services.cache2.clear();
  // Disable rcwn to make cache behavior deterministic.
  await pushPref("network.http.rcwn.enabled", false);

  const {
    getFormattedSize,
  } = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");

  const { tab, monitor } = await initNetMonitor(STATUS_CODES_URL, {
    enableCache: true,
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequestsSummary } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");

  store.dispatch(Actions.batchEnable(false));

  info("Performing requests...");
  const onNetworkEvents = waitForNetworkEvents(monitor, 2);
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    content.wrappedJSObject.performOneCachedRequest();
  });
  info("Wait until we get network events for cached request ");
  await onNetworkEvents;

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
  const totalRequestsCount = state.requests.requests.length;
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
});

// Tests the size for the service worker requests are not included as part
// of the total transferred size.
add_task(async function testTotalTransferredSizeWithServiceWorkerRequests() {
  // Service workers only work on https
  const TEST_URL = HTTPS_EXAMPLE_URL + "service-workers/status-codes.html";
  const { tab, monitor } = await initNetMonitor(TEST_URL, {
    enableCache: true,
    requestCount: 1,
  });
  info("Starting test... ");

  const { store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequestsSummary, getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  info("Performing requests before service worker...");
  await performRequests(monitor, tab, 1);

  info("Registering the service worker...");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await content.wrappedJSObject.registerServiceWorker();
  });

  info("Performing requests which are intercepted by service worker...");
  await performRequests(monitor, tab, 1);

  let displayedServiceWorkerRequests = 0;
  //let totalRequestsTransferredSize = 0;
  let totalRequestsTransferredSizeWithoutServiceWorkers = 0;

  const displayedRequests = getDisplayedRequests(store.getState());

  for (const request of displayedRequests) {
    if (request.fromServiceWorker === true) {
      displayedServiceWorkerRequests++;
    } else {
      totalRequestsTransferredSizeWithoutServiceWorkers +=
        request.transferredSize;
    }
    //totalRequestsTransferredSize += request.transferredSize;
  }

  is(
    displayedServiceWorkerRequests,
    4,
    "Number of service worker requests displayed is correct"
  );

  /* 
    NOTE: Currently only intercepted service worker requests are displayed and the transferred times for these are 
    mostly always zero. Once Bug 1432311 (for fetch requests from the service worker) gets fixed, there should be requests with
    > 0 transfered times, allowing to assert this properly.
    isnot(
      totalRequestsTransferredSize,
      totalRequestsTransferredSizeWithoutServiceWorkers, 
      "The  total transferred size including service worker requests is not equal to the total transferred size excluding service worker requests"
    );
  */

  const requestsSummary = getDisplayedRequestsSummary(store.getState());

  is(
    totalRequestsTransferredSizeWithoutServiceWorkers,
    requestsSummary.transferredSize,
    "The current total transferred size is correct."
  );

  info("Unregistering the service worker...");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await content.wrappedJSObject.unregisterServiceWorker();
  });

  await teardown(monitor);
});
