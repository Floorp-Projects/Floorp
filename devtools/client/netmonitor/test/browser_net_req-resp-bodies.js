/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if request and response body logging stays on after opening the console.
 */

add_task(async function() {
  const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  const { tab, monitor } = await initNetMonitor(JSON_LONG_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  // Perform first batch of requests.
  await performRequests(monitor, tab, 1);

  await verifyRequest(0);

  // Switch to the webconsole.
  const onWebConsole = monitor.toolbox.once("webconsole-selected");
  monitor.toolbox.selectTool("webconsole");
  await onWebConsole;

  // Switch back to the netmonitor.
  const onNetMonitor = monitor.toolbox.once("netmonitor-selected");
  monitor.toolbox.selectTool("netmonitor");
  await onNetMonitor;

  // Reload debugee.
  const wait = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await wait;

  // Perform another batch of requests.
  await performRequests(monitor, tab, 1);

  await verifyRequest(1);

  return teardown(monitor);

  async function verifyRequest(index) {
    const requestItems = document.querySelectorAll(".request-list-item");
    for (const requestItem of requestItems) {
      requestItem.scrollIntoView();
      const requestsListStatus = requestItem.querySelector(".status-code");
      EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
      await waitUntil(() => requestsListStatus.title);
      await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");
    }
    verifyRequestItemTarget(
      document,
      getDisplayedRequests(store.getState()),
      getSortedRequests(store.getState())[index],
      "GET",
      CONTENT_TYPE_SJS + "?fmt=json-long",
      {
        status: 200,
        statusText: "OK",
        type: "json",
        fullMimeType: "text/json; charset=utf-8",
        size: L10N.getFormatStr(
          "networkMenu.size.kB",
          L10N.numberWithDecimals(85975 / 1000, 2)
        ),
        time: true,
      }
    );
  }
});
