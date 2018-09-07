/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UrlClassifierTestUtils } =
  ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const TEST_URI = "http://example.com/browser/devtools/client/" +
                 "netmonitor/test/html_tracking-protection.html";

registerCleanupFunction(function() {
  UrlClassifierTestUtils.cleanupTestTrackers();
});

/**
 * Test that tracking resources are properly marked in the Network panel.
 */
add_task(async function() {
  await UrlClassifierTestUtils.addTestTrackers();

  const { monitor, tab } = await initNetMonitor(TEST_URI);
  info("Starting  test...");

  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute request with third party tracking protection flag.
  await performRequests(monitor, tab, 1);

  const requests = document.querySelectorAll(".request-list-item .tracking-resource");
  is(requests.length, 1, "There should be one tracking request");

  await teardown(monitor);
});
