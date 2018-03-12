/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab shows an error message with broken connections.
 */

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Requesting a resource that has a certificate problem.");

  let requestsDone = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.performRequests(1, "https://nocert.example.com");
  });
  await requestsDone;

  let securityInfoLoaded = waitForDOM(document, ".security-info-value");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".network-details-panel-toggle"));

  await waitUntil(() => document.querySelector("#security-tab"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#security-tab"));
  await securityInfoLoaded;

  let errormsg = document.querySelector(".security-info-value");
  isnot(errormsg.textContent, "", "Error message is not empty.");

  return teardown(monitor);
});
