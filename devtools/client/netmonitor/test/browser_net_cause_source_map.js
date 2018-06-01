/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if request cause is reported correctly when using source maps.
 */

const CAUSE_FILE_NAME = "html_maps-test-page.html";
const CAUSE_URL = EXAMPLE_URL + CAUSE_FILE_NAME;

const N_EXPECTED_REQUESTS = 4;

add_task(async function() {
  // the initNetMonitor function clears the network request list after the
  // page is loaded. That's why we first load a bogus page from SIMPLE_URL,
  // and only then load the real thing from CAUSE_URL - we want to catch
  // all the requests the page is making, not only the XHRs.
  // We can't use about:blank here, because initNetMonitor checks that the
  // page has actually made at least one request.
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));
  let waitPromise = waitForNetworkEvents(monitor, N_EXPECTED_REQUESTS);
  tab.linkedBrowser.loadURI(CAUSE_URL);
  await waitPromise;

  info("Clicking item and waiting for details panel to open");
  waitPromise = waitForDOM(document, ".network-details-panel");
  const xhrRequestItem = document.querySelectorAll(".request-list-item")[3];
  EventUtils.sendMouseEvent({ type: "mousedown" }, xhrRequestItem);
  await waitPromise;

  info("Clicking stack tab and waiting for stack panel to open");
  waitPromise = waitForDOM(document, "#stack-trace-panel");
  const stackTab = document.querySelector("#stack-trace-tab");
  EventUtils.sendMouseEvent({ type: "click" }, stackTab);
  await waitPromise;

  info("Waiting for source maps to be applied");
  await waitUntil(() => {
    const frames = document.querySelectorAll(".frame-link");
    return frames && frames.length >= 2 &&
      frames[0].textContent.includes("xhr_original") &&
      frames[1].textContent.includes("xhr_original");
  });

  const frames = document.querySelectorAll(".frame-link");
  is(frames.length, 3, "should have 3 stack frames");
  is(frames[0].textContent, `reallydoxhr xhr_original.js:6`);
  is(frames[1].textContent, `doxhr xhr_original.js:10`);

  await teardown(monitor);
});
