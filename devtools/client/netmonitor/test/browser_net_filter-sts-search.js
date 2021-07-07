/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test incomplete status-code search
 */
const REQUESTS = [
  { url: "sjs_status-codes-test-server.sjs?sts=400" },
  { url: "sjs_status-codes-test-server.sjs?sts=300" },
  { url: "sjs_status-codes-test-server.sjs?sts=304" },
];

add_task(async function() {
  const { monitor } = await initNetMonitor(FILTERING_URL, { requestCount: 1 });
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  const { getDisplayedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  info("Starting test... ");

  // Let the requests load completely before the incomplete search tests begin
  // because we are searching for the status code in these requests.
  const waitNetwork = waitForNetworkEvents(monitor, REQUESTS.length);
  await performRequestsInContent(REQUESTS);
  await waitNetwork;

  document.querySelector(".devtools-filterinput").focus();

  EventUtils.sendString("status-code:3");

  let visibleItems = getDisplayedRequests(store.getState());

  // Results will be updated asynchronously, so we should wait until
  // displayed requests reach final state.
  await waitUntil(() => {
    visibleItems = getDisplayedRequests(store.getState());
    return visibleItems.length === 2;
  });

  is(
    Number(visibleItems[0].status),
    303,
    "First visible item has expected status"
  );
  is(
    Number(visibleItems[1].status),
    304,
    "Second visible item has expected status"
  );

  await teardown(monitor);
});
