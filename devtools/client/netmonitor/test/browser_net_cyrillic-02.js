/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if cyrillic text is rendered correctly in the source editor
 * when loaded directly from an HTML page.
 */

add_task(async function () {
  const { monitor } = await initNetMonitor(CYRILLIC_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequests, getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  let wait = waitForNetworkEvents(monitor, 1);
  await reloadBrowser();
  await wait;

  const requestItem = document.querySelectorAll(".request-list-item")[0];
  const requestsListStatus = requestItem.querySelector(".status-code");
  requestItem.scrollIntoView();
  EventUtils.sendMouseEvent({ type: "mouseover" }, requestsListStatus);
  await waitUntil(() => requestsListStatus.title);
  await waitForDOMIfNeeded(requestItem, ".requests-list-timings-total");

  verifyRequestItemTarget(
    document,
    getDisplayedRequests(store.getState()),
    getSortedRequests(store.getState())[0],
    "GET",
    CYRILLIC_URL,
    {
      status: 200,
      statusText: "OK",
    }
  );

  wait = waitForDOM(document, "#headers-panel");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  wait = waitForDOM(document, "#response-panel .data-header");
  clickOnSidebarTab(document, "response");
  await wait;

  wait = waitForDOM(document, "#response-panel .CodeMirror-code");
  const header = document.querySelector(
    "#response-panel .raw-data-toggle-input .devtools-checkbox-toggle"
  );
  clickElement(header, monitor);
  await wait;

  // CodeMirror will only load lines currently in view to the DOM. getValue()
  // retrieves all lines pending render after a user begins scrolling.
  const text = document.querySelector(".CodeMirror").CodeMirror.getValue();

  ok(
    text.includes("\u0411\u0440\u0430\u0442\u0430\u043d"),
    "The text shown in the source editor is correct."
  );

  return teardown(monitor);
});
