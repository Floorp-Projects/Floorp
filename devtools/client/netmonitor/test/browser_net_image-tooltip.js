/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if image responses show a popup in the requests menu when hovered.
 */

function test() {
  initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $, EVENTS, ACTIVITY_TYPE, NetMonitorView, NetMonitorController } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    promise.all([
      waitForNetworkEvents(aMonitor, 7),
      waitFor(aMonitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
    ]).then(() => {
      info("Checking the image thumbnail after a few requests were made...");
      let requestItem = RequestsMenu.items[5];
      let requestTooltip = requestItem.attachment.tooltip;
      ok(requestTooltip, "There should be a tooltip instance for the image request.");

      let anchor = $(".requests-menu-file", requestItem.target);
      return showTooltipOn(requestTooltip, anchor);
    }).then(aTooltip => {
      ok(true,
        "An tooltip was successfully opened for the image request.");
      is(aTooltip.content.querySelector("image").src, TEST_IMAGE_DATA_URI,
        "The tooltip's image content is displayed correctly.");

      info("Reloading the debuggee and performing all requests again...");
      reloadAndPerformRequests();

      return promise.all([
        waitForNetworkEvents(aMonitor, 7), // 6 + 1
        waitFor(aMonitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED)
      ]);
    }).then(() => {
      info("Checking the image thumbnail after a reload.");
      let requestItem = RequestsMenu.items[6];
      let requestTooltip = requestItem.attachment.tooltip;
      ok(requestTooltip, "There should be a tooltip instance for the image request.");

      let anchor = $(".requests-menu-file", requestItem.target);
      return showTooltipOn(requestTooltip, anchor);
    }).then(aTooltip => {
      ok(true,
        "An tooltip was successfully opened for the image request.");
      is(aTooltip.content.querySelector("image").src, TEST_IMAGE_DATA_URI,
        "The tooltip's image content is displayed correctly.");

      teardown(aMonitor).then(finish);
    });

    function reloadAndPerformRequests() {
      NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED).then(() => {
        aDebuggee.performRequests();
      });
    }

    /**
     * @return a promise that resolves when the tooltip is shown
     */
    function showTooltipOn(tooltip, element) {
      return Task.spawn(function*() {
        let isValidTarget = yield tooltip._toggle.isValidHoverTarget(element);
        ok(isValidTarget, "Element is a valid tooltip target");
        let onShown = tooltip.once("shown");
        tooltip.show();
        yield onShown;
        return tooltip;
      });
    }

    aDebuggee.performRequests();
  });
}
