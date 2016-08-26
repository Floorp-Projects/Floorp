/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that clicking on the security indicator opens the security details tab.
 */

add_task(function* () {
  let [tab, , monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Requesting a resource over HTTPS.");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_2");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_1");

  is(RequestsMenu.itemCount, 2, "Two events event logged.");

  yield clickAndTestSecurityIcon();

  info("Selecting headers panel again.");
  NetworkDetails.widget.selectedIndex = 0;
  yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

  info("Sorting the items by filename.");
  EventUtils.sendMouseEvent({ type: "click" }, $("#requests-menu-file-button"));

  info("Testing that security icon can be clicked after the items were sorted.");
  yield clickAndTestSecurityIcon();

  return teardown(monitor);

  function* performRequestAndWait(url) {
    let wait = waitForNetworkEvents(monitor, 1);
    yield ContentTask.spawn(tab.linkedBrowser, { url }, function* (args) {
      content.wrappedJSObject.performRequests(1, args.url);
    });
    return wait;
  }

  function* clickAndTestSecurityIcon() {
    let item = RequestsMenu.items[0];
    let icon = $(".requests-security-state-icon", item.target);

    info("Clicking security icon of the first request and waiting for the " +
         "panel to update.");

    icon.click();
    yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

    is(NetworkDetails.widget.selectedPanel, $("#security-tabpanel"),
      "Security tab is selected.");
  }
});
