/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the  Request payload section is collapsed when the JSON section
 * is avaliable.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(POST_ARRAY_DATA_URL);
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute requests.
  await performRequests(monitor, tab, 1);

  const wait = waitForDOM(document, ".headers-overview");
  EventUtils.sendMouseEvent(
    { type: "mousedown" },
    document.querySelectorAll(".request-list-item")[0]
  );
  await wait;

  EventUtils.sendMouseEvent(
    { type: "click" },
    document.querySelector("#params-tab")
  );

  await waitForDOM(document, "#params-panel .accordion-item", 2);

  const panel = document.querySelector("#params-panel");

  const jsonSection = panel.querySelector(".accordion-item#jsonScopeName");
  const requestPayloadSection = panel.querySelector(
    ".accordion-item#paramsPostPayload"
  );

  is(
    !!jsonSection.querySelector(".accordion-header[aria-expanded='true']"),
    true,
    "JSON Accordion item should not be collapsed"
  );

  is(
    !!requestPayloadSection.querySelector(
      ".accordion-header[aria-expanded='true']"
    ),
    false,
    "Request Payload Accordion item should be collapsed"
  );

  await teardown(monitor);
});
