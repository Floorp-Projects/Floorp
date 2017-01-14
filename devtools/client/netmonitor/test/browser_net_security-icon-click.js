/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clicking on the security indicator opens the security details tab.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  info("Requesting a resource over HTTPS.");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_2");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_1");

  is(RequestsMenu.itemCount, 2, "Two events event logged.");

  yield clickAndTestSecurityIcon();

  info("Selecting headers panel again.");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector("#tab-0 a"));

  info("Sorting the items by filename.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-file-button"));

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
    let item = RequestsMenu.getItemAtIndex(0);
    let icon = document.querySelector(".requests-security-state-icon");

    let wait = waitForDOM(document, "#panel-5");
    info("Clicking security icon of the first request and waiting for panel update.");
    EventUtils.synthesizeMouseAtCenter(icon, {}, monitor.panelWin);
    yield wait;
    ok(document.querySelector("#tab-5.is-active"), "Security tab is selected.");
  }
});
