/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the summary text displayed in the network requests menu footer is correct.
 */

add_task(function* () {
  requestLongerTimeout(2);

  let { tab, monitor } = yield initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  let { $, NetMonitorView, gStore, windowRequire } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  let { getDisplayedRequestsSummary } =
    windowRequire("devtools/client/netmonitor/selectors/index");
  let { L10N } = windowRequire("devtools/client/netmonitor/l10n");
  let { PluralForm } = windowRequire("devtools/shared/plural-form");

  RequestsMenu.lazyUpdate = false;
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
      let buttonEl = $(`#requests-menu-filter-${button}-button`);
      EventUtils.sendMouseEvent({ type: "click" }, buttonEl);
      testStatus();
    }
  }

  yield teardown(monitor);

  function testStatus() {
    let value = $("#requests-menu-network-summary-button").textContent;
    info("Current summary: " + value);

    let state = gStore.getState();
    let totalRequestsCount = state.requests.requests.size;
    let requestsSummary = getDisplayedRequestsSummary(state);
    info(`Current requests: ${requestsSummary.count} of ${totalRequestsCount}.`);

    if (!totalRequestsCount || !requestsSummary.count) {
      is(value, L10N.getStr("networkMenu.empty"),
        "The current summary text is incorrect, expected an 'empty' label.");
      return;
    }

    info(`Computed total bytes: ${requestsSummary.bytes}`);
    info(`Computed total millis: ${requestsSummary.millis}`);

    is(value, PluralForm.get(requestsSummary.count, L10N.getStr("networkMenu.summary"))
      .replace("#1", requestsSummary.count)
      .replace("#2", L10N.numberWithDecimals(requestsSummary.bytes / 1024, 2))
      .replace("#3", L10N.numberWithDecimals(requestsSummary.millis / 1000, 2))
    , "The current summary text is correct.");
  }
});
