/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test if the New Request panel shows in the left when the pref is true
 */

add_task(async function() {
  const { monitor } = await initNetMonitor(HTTPS_CUSTOM_GET_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  const { document, store, windowRequire } = monitor.panelWin;

  // Action should be processed synchronously in tests.
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  store.dispatch(Actions.batchEnable(false));

  info("open the left panel");
  let waitForPanels = waitForDOM(
    document,
    ".monitor-panel .network-action-bar"
  );

  const respSearchButton = document.querySelector(
    "#netmonitor-toolbar-container .devtools-search-icon"
  );
  respSearchButton.click();
  await waitForPanels;
  is(
    !!document.querySelector("#network-action-bar-HTTP-custom-request-panel"),
    false,
    "The 'New Request' header should be hidden when the pref is false."
  );

  // Close the panel before changing pref
  const closePanel = document.querySelector(
    ".network-action-bar .tabs-navigation .sidebar-toggle"
  );
  closePanel.click();

  await pushPref("devtools.netmonitor.features.newEditAndResend", true);

  info("open the left panel");
  waitForPanels = waitForDOM(document, ".monitor-panel .network-action-bar");
  respSearchButton.click();
  await waitForPanels;

  info("switching to new request panel");
  is(
    !!document.querySelector("#network-action-bar-HTTP-custom-request-panel"),
    true,
    "The 'New Request' header should be visible when the pref is true."
  );

  await teardown(monitor);
});
