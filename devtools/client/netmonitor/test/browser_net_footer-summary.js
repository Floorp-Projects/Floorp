/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the summary text displayed in the network requests menu footer is correct.
 */

add_task(function* () {
  const {
    getFormattedSize,
    getFormattedTime
  } = require("devtools/client/netmonitor/src/utils/format-utils");

  requestLongerTimeout(2);

  let { tab, monitor } = yield initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { getDisplayedRequestsSummary } =
    windowRequire("devtools/client/netmonitor/src/selectors/index");
  let { L10N } = windowRequire("devtools/client/netmonitor/src/utils/l10n");
  let { PluralForm } = windowRequire("devtools/shared/plural-form");

  store.dispatch(Actions.batchEnable(false));
  testStatus();

  for (let i = 0; i < 2; i++) {
    info(`Performing requests in batch #${i}`);
    let wait = waitForNetworkEvents(monitor, 8);
    yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.wrappedJSObject.performRequests('{ "getMedia": true, "getFlash": true }');
    });
    yield wait;

    testStatus();

    let buttons = ["html", "css", "js", "xhr", "fonts", "images", "media", "flash"];
    for (let button of buttons) {
      let buttonEl = document.querySelector(`.requests-list-filter-${button}-button`);
      EventUtils.sendMouseEvent({ type: "click" }, buttonEl);
      testStatus();
    }
  }

  yield teardown(monitor);

  function testStatus() {
    let state = store.getState();
    let totalRequestsCount = state.requests.requests.size;
    let requestsSummary = getDisplayedRequestsSummary(state);
    info(`Current requests: ${requestsSummary.count} of ${totalRequestsCount}.`);

    let valueCount = document.querySelector(".requests-list-network-summary-count")
                        .textContent;
    info("Current summary count: " + valueCount);
    let expectedCount = PluralForm.get(requestsSummary.count,
      L10N.getFormatStrWithNumbers("networkMenu.summary.requestsCount",
        requestsSummary.count));

    if (!totalRequestsCount || !requestsSummary.count) {
      is(valueCount, L10N.getStr("networkMenu.summary.requestsCountEmpty"),
        "The current summary text is incorrect, expected an 'empty' label.");
      return;
    }

    let valueTransfer = document.querySelector(".requests-list-network-summary-transfer")
                        .textContent;
    info("Current summary transfer: " + valueTransfer);
    let expectedTransfer = L10N.getFormatStrWithNumbers("networkMenu.summary.transferred",
      getFormattedSize(requestsSummary.contentSize),
      getFormattedSize(requestsSummary.transferredSize));

    let valueFinish = document.querySelector(".requests-list-network-summary-finish")
                        .textContent;
    info("Current summary finish: " + valueFinish);
    let expectedFinish = L10N.getFormatStrWithNumbers("networkMenu.summary.finish",
      getFormattedTime(requestsSummary.millis));

    info(`Computed total bytes: ${requestsSummary.bytes}`);
    info(`Computed total millis: ${requestsSummary.millis}`);

    is(valueCount, expectedCount, "The current summary count is correct.");
    is(valueTransfer, expectedTransfer, "The current summary transfer is correct.");
    is(valueFinish, expectedFinish, "The current summary finish is correct.");
  }
});
