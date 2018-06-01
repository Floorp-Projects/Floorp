/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab shows an error message with broken connections.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Requesting a resource that has a certificate problem.");

  const requestsDone = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.performRequests(1, "https://nocert.example.com");
  });
  await requestsDone;

  const securityInfoLoaded = waitForDOM(document, ".security-info-value");
  store.dispatch(Actions.toggleNetworkDetails());

  await waitUntil(() => document.querySelector("#security-tab"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#security-tab"));
  await securityInfoLoaded;

  const errormsg = document.querySelector(".security-info-value");
  isnot(errormsg.textContent, "", "Error message is not empty.");

  return teardown(monitor);
});
