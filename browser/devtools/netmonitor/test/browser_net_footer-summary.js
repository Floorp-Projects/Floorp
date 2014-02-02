/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test if the summary text displayed in the network requests menu footer
 * is correct.
 */

function test() {
  let { PluralForm } = Cu.import("resource://gre/modules/PluralForm.jsm", {});

  initNetMonitor(FILTERING_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;
    testStatus();

    waitForNetworkEvents(aMonitor, 8).then(() => {
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-xhr-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-fonts-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-images-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-media-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
      testStatus();

      info("Performing more requests.");
      aDebuggee.performRequests('{ "getMedia": true, "getFlash": true }');
      return waitForNetworkEvents(aMonitor, 8);
    })
    .then(() => {
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-html-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-css-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-js-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-xhr-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-fonts-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-images-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-media-button"));
      testStatus();

      EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-filter-flash-button"));
      testStatus();

      teardown(aMonitor).then(finish);
    })

    function testStatus() {
      let summary = $("#requests-menu-network-summary-label");
      let value = summary.getAttribute("value");
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
      , "The current summary text is incorrect.")
    }

    aDebuggee.performRequests('{ "getMedia": true, "getFlash": true }');
  });
}
