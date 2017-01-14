/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that warning indicators are shown when appropriate.
 */

const TEST_CASES = [
  {
    desc: "no warnings",
    uri: "https://example.com" + CORS_SJS_PATH,
    warnCipher: null,
  },
];

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  for (let test of TEST_CASES) {
    info("Testing site with " + test.desc);

    info("Performing request to " + test.uri);
    let wait = waitForNetworkEvents(monitor, 1);
    yield ContentTask.spawn(tab.linkedBrowser, test.uri, function* (url) {
      content.wrappedJSObject.performRequests(1, url);
    });
    yield wait;

    info("Selecting the request.");
    wait = waitForDOM(document, ".tabs");
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[0]);
    yield wait;

    if (!document.querySelector("#tab-5.is-active")) {
      info("Selecting security tab.");
      wait = waitForDOM(document, "#panel-5 .properties-view");
      document.querySelector("#tab-5 a").click();
      yield wait;
    }

    is(document.querySelector("#security-warning-cipher"),
      test.warnCipher,
      "Cipher suite warning is hidden.");

    RequestsMenu.clear();
  }

  return teardown(monitor);
});
