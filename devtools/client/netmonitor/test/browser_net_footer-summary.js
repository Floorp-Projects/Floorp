/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the summary text displayed in the network requests menu footer
 * is correct.
 */

add_task(function* () {
  requestLongerTimeout(2);
  let { PluralForm } = Cu.import("resource://gre/modules/PluralForm.jsm", {});

  let { tab, monitor } = yield initNetMonitor(FILTERING_URL);
  info("Starting test... ");

  let { $, L10N, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

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
    let summary = $("#requests-menu-network-summary-button");
    let value = summary.getAttribute("label");
    info("Current summary: " + value);

    let visibleItems = RequestsMenu.visibleItems;
    let visibleRequestsCount = visibleItems.length;
    let totalRequestsCount = RequestsMenu.itemCount;
    info("Current requests: " + visibleRequestsCount + " of " + totalRequestsCount + ".");

    if (!totalRequestsCount || !visibleRequestsCount) {
      is(value, L10N.getStr("networkMenu.empty"),
        "The current summary text is incorrect, expected an 'empty' label.");
      return;
    }

    let totalBytes = RequestsMenu._getTotalBytesOfRequests(visibleItems);
    let totalMillis =
      RequestsMenu._getNewestRequest(visibleItems).attachment.endedMillis -
      RequestsMenu._getOldestRequest(visibleItems).attachment.startedMillis;

    info("Computed total bytes: " + totalBytes);
    info("Computed total millis: " + totalMillis);

    is(value, PluralForm.get(visibleRequestsCount, L10N.getStr("networkMenu.summary"))
      .replace("#1", visibleRequestsCount)
      .replace("#2", L10N.numberWithDecimals((totalBytes || 0) / 1024, 2))
      .replace("#3", L10N.numberWithDecimals((totalMillis || 0) / 1000, 2))
    , "The current summary text is incorrect.");
  }
});
