/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if image responses show a popup in the requests menu when hovered.
 */
add_task(function* test() {
  let [, debuggee, monitor] = yield initNetMonitor(CONTENT_TYPE_WITHOUT_CACHE_URL);
  info("Starting test... ");

  let { $, EVENTS, ACTIVITY_TYPE, NetMonitorView, NetMonitorController } =
    monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = true;

  let onEvents = waitForNetworkEvents(monitor, 7);
  let onThumbnail = waitFor(monitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);

  debuggee.performRequests();
  yield onEvents;
  yield onThumbnail;

  info("Checking the image thumbnail after a few requests were made...");
  yield showTooltipAndVerify(RequestsMenu.tooltip, RequestsMenu.items[5]);

  // 7 XHRs as before + 1 extra document reload
  onEvents = waitForNetworkEvents(monitor, 8);
  onThumbnail = waitFor(monitor.panelWin, EVENTS.RESPONSE_IMAGE_THUMBNAIL_DISPLAYED);

  info("Reloading the debuggee and performing all requests again...");
  yield NetMonitorController.triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  debuggee.performRequests();
  yield onEvents;
  yield onThumbnail;

  info("Checking the image thumbnail after a reload.");
  yield showTooltipAndVerify(RequestsMenu.tooltip, RequestsMenu.items[6]);

  info("Checking if the image thumbnail is hidden when mouse leaves the menu widget");
  let requestsMenuEl = $("#requests-menu-contents");
  let onHidden = RequestsMenu.tooltip.once("hidden");
  EventUtils.synthesizeMouse(requestsMenuEl, 0, 0, {type: "mouseout"}, monitor.panelWin);
  yield onHidden;

  yield teardown(monitor);
  finish();

  /**
   * Show a tooltip on the {requestItem} and verify that it was displayed
   * with the expected content.
   */
  function* showTooltipAndVerify(tooltip, requestItem) {
    let anchor = $(".requests-menu-file", requestItem.target);
    yield showTooltipOn(tooltip, anchor);

    info("Tooltip was successfully opened for the image request.");
    is(tooltip.panel.querySelector("img").src, TEST_IMAGE_DATA_URI,
      "The tooltip's image content is displayed correctly.");
  }

  /**
   * Trigger a tooltip over an element by sending mousemove event.
   * @return a promise that resolves when the tooltip is shown
   */
  function showTooltipOn(tooltip, element) {
    let onShown = tooltip.once("shown");
    let win = element.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(element, {type: "mousemove"}, win);
    return onShown;
  }
});
