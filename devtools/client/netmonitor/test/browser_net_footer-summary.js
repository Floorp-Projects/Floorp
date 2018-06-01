/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the summary text displayed in the network requests menu footer is correct.
 */

add_task(async function() {
  const {
    getFormattedSize,
    getFormattedTime
  } = require("devtools/client/netmonitor/src/utils/format-utils");

  requestLongerTimeout(2);

  const { tab, monitor } = await initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { getDisplayedRequestsSummary } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");
  const { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  const { PluralForm } = windowRequire("devtools/shared/plural-form");

  store.dispatch(Actions.batchEnable(false));
  testStatus();

  for (let i = 0; i < 2; i++) {
    info(`Performing requests in batch #${i}`);
    const wait = waitForNetworkEvents(monitor, 8);
    await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
      content.wrappedJSObject.performRequests('{ "getMedia": true, "getFlash": true }');
    });
    await wait;

    testStatus();

    const buttons = ["html", "css", "js", "xhr", "fonts", "images", "media"];
    for (const button of buttons) {
      const buttonEl = document.querySelector(`.requests-list-filter-${button}-button`);
      EventUtils.sendMouseEvent({ type: "click" }, buttonEl);
      testStatus();
    }
  }

  await teardown(monitor);

  function testStatus() {
    const state = store.getState();
    const totalRequestsCount = state.requests.requests.size;
    const requestsSummary = getDisplayedRequestsSummary(state);
    info(`Current requests: ${requestsSummary.count} of ${totalRequestsCount}.`);

    const valueCount = document.querySelector(".requests-list-network-summary-count")
                        .textContent;
    info("Current summary count: " + valueCount);
    const expectedCount = PluralForm.get(requestsSummary.count,
      L10N.getStr("networkMenu.summary.requestsCount2"))
        .replace("#1", requestsSummary.count);

    if (!totalRequestsCount || !requestsSummary.count) {
      is(valueCount, L10N.getStr("networkMenu.summary.requestsCountEmpty"),
        "The current summary text is incorrect, expected an 'empty' label.");
      return;
    }

    const valueTransfer =
      document.querySelector(".requests-list-network-summary-transfer").textContent;
    info("Current summary transfer: " + valueTransfer);
    const expectedTransfer = L10N.getFormatStrWithNumbers(
      "networkMenu.summary.transferred",
      getFormattedSize(requestsSummary.contentSize),
      getFormattedSize(requestsSummary.transferredSize));

    const valueFinish = document.querySelector(".requests-list-network-summary-finish")
                        .textContent;
    info("Current summary finish: " + valueFinish);
    const expectedFinish = L10N.getFormatStrWithNumbers("networkMenu.summary.finish",
      getFormattedTime(requestsSummary.millis));

    info(`Computed total bytes: ${requestsSummary.bytes}`);
    info(`Computed total millis: ${requestsSummary.millis}`);

    is(valueCount, expectedCount, "The current summary count is correct.");
    is(valueTransfer, expectedTransfer, "The current summary transfer is correct.");
    is(valueFinish, expectedFinish, "The current summary finish is correct.");
  }
});
