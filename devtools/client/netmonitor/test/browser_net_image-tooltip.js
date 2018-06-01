/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const IMAGE_TOOLTIP_URL = EXAMPLE_URL + "html_image-tooltip-test-page.html";
const IMAGE_TOOLTIP_REQUESTS = 1;

/**
 * Tests if image responses show a popup in the requests menu when hovered.
 */
add_task(async function test() {
  const { tab, monitor } = await initNetMonitor(IMAGE_TOOLTIP_URL);
  info("Starting test... ");

  const { document, store, windowRequire, connector } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { triggerActivity } = connector;
  const { ACTIVITY_TYPE } = windowRequire("devtools/client/netmonitor/src/constants");
  const toolboxDoc = monitor.panelWin.parent.document;

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, IMAGE_TOOLTIP_REQUESTS);

  info("Checking the image thumbnail after a few requests were made...");
  await showTooltipAndVerify(document.querySelectorAll(".request-list-item")[0]);

  // Hide tooltip before next test, to avoid the situation that tooltip covers
  // the icon for the request of the next test.
  info("Checking the image thumbnail gets hidden...");
  await hideTooltipAndVerify(document.querySelectorAll(".request-list-item")[0]);

  // +1 extra document reload
  const onEvents = waitForNetworkEvents(monitor, IMAGE_TOOLTIP_REQUESTS + 1);

  info("Reloading the debuggee and performing all requests again...");
  await triggerActivity(ACTIVITY_TYPE.RELOAD.WITH_CACHE_ENABLED);
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.performRequests();
  });
  await onEvents;

  info("Checking the image thumbnail after a reload.");
  await showTooltipAndVerify(document.querySelectorAll(".request-list-item")[1]);

  info("Checking if the image thumbnail is hidden when mouse leaves the menu widget");
  const requestsListContents = document.querySelector(".requests-list-contents");
  EventUtils.synthesizeMouse(requestsListContents, 0, 0, { type: "mousemove" },
                             monitor.panelWin);
  await waitUntil(() => !toolboxDoc.querySelector(".tooltip-container.tooltip-visible"));

  await teardown(monitor);

  /**
   * Show a tooltip on the {target} and verify that it was displayed
   * with the expected content.
   */
  async function showTooltipAndVerify(target) {
    const anchor = target.querySelector(".requests-list-file");
    await showTooltipOn(anchor);

    info("Tooltip was successfully opened for the image request.");
    is(toolboxDoc.querySelector(".tooltip-panel img").src, TEST_IMAGE_DATA_URI,
      "The tooltip's image content is displayed correctly.");
  }

  /**
   * Trigger a tooltip over an element by sending mousemove event.
   * @return a promise that resolves when the tooltip is shown
   */
  async function showTooltipOn(element) {
    const win = element.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(element, { type: "mousemove" }, win);
    await waitUntil(() => toolboxDoc.querySelector(".tooltip-panel img"));
  }

  /**
   * Hide a tooltip on the {target} and verify that it was closed.
   */
  async function hideTooltipAndVerify(target) {
    // Hovering over the "method" column hides the tooltip.
    const anchor = target.querySelector(".requests-list-method");
    const win = anchor.ownerDocument.defaultView;
    EventUtils.synthesizeMouseAtCenter(anchor, { type: "mousemove" }, win);

    await waitUntil(
      () => !toolboxDoc.querySelector(".tooltip-container.tooltip-visible"));
    info("Tooltip was successfully closed.");
  }
});
