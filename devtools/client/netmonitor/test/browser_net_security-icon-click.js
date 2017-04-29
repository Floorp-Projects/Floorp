/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that clicking on the security indicator opens the security details tab.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Requesting a resource over HTTPS.");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_2");
  yield performRequestAndWait("https://example.com" + CORS_SJS_PATH + "?request_1");

  is(store.getState().requests.requests.size, 2, "Two events event logged.");

  yield clickAndTestSecurityIcon();

  info("Selecting headers panel again.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#headers-tab"));

  info("Sorting the items by filename.");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#requests-list-file-button"));

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
    let icon = document.querySelector(".requests-security-state-icon");

    info("Clicking security icon of the first request and waiting for panel update.");
    EventUtils.synthesizeMouseAtCenter(icon, {}, monitor.panelWin);

    ok(document.querySelector("#security-tab[aria-selected=true]"),
       "Security tab is selected.");
  }
});
