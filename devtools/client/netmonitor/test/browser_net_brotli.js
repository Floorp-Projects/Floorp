/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BROTLI_URL = HTTPS_EXAMPLE_URL + "html_brotli-test-page.html";
const BROTLI_REQUESTS = 1;

/**
 * Test brotli encoded response is handled correctly on HTTPS.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(BROTLI_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, BROTLI_REQUESTS);

  const requestItem = document.querySelector(".request-list-item");
  // Status code title is generated on hover
  const requestsListStatus = requestItem.querySelector(".status-code");
  EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
  await waitUntil(() => requestsListStatus.title);
  await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[0],
    "GET",
    HTTPS_CONTENT_TYPE_SJS + "?fmt=br",
    {
      status: 200,
      statusText: "Connected",
      type: "plain",
      fullMimeType: "text/plain",
      transferred: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 60),
      size: L10N.getFormatStrWithNumbers("networkMenu.sizeB", 64),
      time: true,
    }
  );

  const wait = waitForDOM(document, ".CodeMirror-code");
  const onResponseContent = monitor.panelWin.api.once(
    TEST_EVENTS.RECEIVED_RESPONSE_CONTENT
  );
  store.dispatch(Actions.toggleNetworkDetails());
  clickOnSidebarTab(document, "response");
  await wait;
  await onResponseContent;
  await testResponse("br");
  await teardown(monitor);

  function testResponse(type) {
    switch (type) {
      case "br": {
        is(
          getCodeMirrorValue(monitor),
          "X".repeat(64),
          "The text shown in the source editor is incorrect for the brotli request."
        );
        break;
      }
    }
  }
});
