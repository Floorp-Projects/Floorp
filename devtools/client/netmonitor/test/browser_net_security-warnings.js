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

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  for (const test of TEST_CASES) {
    info("Testing site with " + test.desc);

    info("Performing request to " + test.uri);
    let wait = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, test.uri, async function(url) {
      content.wrappedJSObject.performRequests(1, url);
    });
    await wait;

    info("Selecting the request.");
    wait = waitForDOM(document, ".tabs");
    EventUtils.sendMouseEvent({ type: "mousedown" },
      document.querySelectorAll(".request-list-item")[0]);
    await wait;

    if (!document.querySelector("#security-tab[aria-selected=true]")) {
      info("Selecting security tab.");
      wait = waitForDOM(document, "#security-panel .properties-view");
      EventUtils.sendMouseEvent({ type: "click" },
        document.querySelector("#security-tab"));
      await wait;
    }

    is(document.querySelector("#security-warning-cipher"),
      test.warnCipher,
      "Cipher suite warning is hidden.");

    store.dispatch(Actions.clearRequests());
  }

  return teardown(monitor);
});
