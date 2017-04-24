/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const IMAGE_TOOLTIP_URL = EXAMPLE_URL + "html_image-tooltip-test-page.html";
const IMAGE_TOOLTIP_REQUESTS = 1;

/**
 * Tests if image responses show a popup in the requests menu when hovered.
 */
add_task(function* test() {
  let { tab, monitor } = yield initNetMonitor(IMAGE_TOOLTIP_URL);
  info("Starting test... ");

  let { document, gStore, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let { NetMonitorController } =
    windowRequire("devtools/client/netmonitor/src/netmonitor-controller");
  let {
    ACTIVITY_TYPE,
    EVENTS,
  } = windowRequire("devtools/client/netmonitor/src/constants");
  let toolboxDoc = monitor.panelWin.parent.document;

  gStore.dispatch(Actions.batchEnable(false));

  let onEvents = waitForNetworkEvents(monitor, IMAGE_TOOLTIP_REQUESTS);
  let onThumbnail = monitor.panelWin.once(EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);
  yield performRequests();
  yield onEvents;
  yield onThumbnail;

  info("Checking the image thumbnail after a few requests were made...");
  yield showTooltipAndVerify(document.querySelectorAll(".request-list-item")[0]);

  // Hide tooltip before next test, to avoid the situation that tooltip covers
  // the icon for the request of the next test.
  info("Checking the image thumbnail gets hidden...");
  yield hideTooltipAndVerify(document.querySelectorAll(".request-list-item")[0]);

  // +1 extra document reload
  onEvents = waitForNetworkEvents(monitor, IMAGE_TOOLTIP_REQUESTS + 1);
  onThumbnail = monitor.panelWin.once(EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);

  info("Reloading the debuggee and performing all requests again...");
  yield NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  yield performRequests();
  yield onEvents;
  yield onThumbnail;

  info("Checking the image thumbnail after a reload.");
  yield showTooltipAndVerify(document.querySelectorAll(".request-list-item")[1]);

  info("Checking if the image thumbnail is hidden when mouse leaves the menu widget");
  let requestsListContents = document.querySelector(".requests-list-contents");
  EventUtils.synthesizeMouse(requestsListContents, 0, 0, { type: "mouseout" },
                             monitor.panelWin);
  yield waitUntil(() => !toolboxDoc.querySelector(".tooltip-container.tooltip-visible"));

  yield teardown(monitor);

  function performRequests() {
    return ContentTask.spawn(tab.linkedBrowser, {}, function* () {
      content.wrappedJSObject.performRequests();
    });
  }

  /**
   * Show a tooltip on the {target} and verify that it was displayed
   * with the expected content.
   */
  function* showTooltipAndVerify(target) {
    let anchor = target.querySelector(".requests-list-icon");
    yield showTooltipOn(anchor);

    info("Tooltip was successfully opened for the image request.");
    is(toolboxDoc.querySelector(".tooltip-panel img").src, TEST_IMAGE_DATA_URI,
      "The tooltip's image content is displayed correctly.");
  }

  /**
   * Trigger a tooltip over an element by sending mousemove event.
   * @return a promise that resolves when the tooltip is shown
   */
  function* showTooltipOn(element) {
    let win = element.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(element, { type: "mousemove" }, win);
    yield waitUntil(() => toolboxDoc.querySelector(".tooltip-panel img"));
  }

  /**
   * Hide a tooltip on the {target} and verify that it was closed.
   */
  function* hideTooltipAndVerify(target) {
    // Hovering over the "method" column hides the tooltip.
    let anchor = target.querySelector(".requests-list-method");
    let win = anchor.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(anchor, { type: "mousemove" }, win);

    yield waitUntil(
      () => !toolboxDoc.querySelector(".tooltip-container.tooltip-visible"));
    info("Tooltip was successfully closed.");
  }
});
