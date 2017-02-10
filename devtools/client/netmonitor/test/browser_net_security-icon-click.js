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
console.log(123)
  yield clickAndTestSecurityIcon();
console.log(123)

  info("Selecting headers panel again.");
  EventUtils.sendMouseEvent({ type: "mousedown" },
    document.querySelector("#headers-tab"));

  info("Sorting the items by filename.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-menu-file-button"));

  info("Testing that security icon can be clicked after the items were sorted.");
console.log(123)
  yield clickAndTestSecurityIcon();
console.log(123)

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

    info("Clicking security icon of the first request and waiting for panel update.");
    EventUtils.synthesizeMouseAtCenter(icon, {}, monitor.panelWin);

    ok(document.querySelector("#security-tab[aria-selected=true]"), "Security tab is selected.");
  }
});
