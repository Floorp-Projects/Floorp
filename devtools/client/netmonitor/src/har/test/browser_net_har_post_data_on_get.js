/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for exporting POST data into HAR format.
 */
add_task(async function() {
  const { tab, monitor } = await initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_post-data-test-page.html");

  info("Starting test... ");

  const { connector, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { HarMenuUtils } = windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils");
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute one GET request on the page and wait till its done.
  const wait = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    content.wrappedJSObject.executeTest3();
  });
  await wait;

  // Copy HAR into the clipboard (asynchronous).
  const jsonString = await HarMenuUtils.copyAllAsHar(
    getSortedRequests(store.getState()), connector);
  const har = JSON.parse(jsonString);

  // Check out the HAR log.
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  const entry = har.log.entries[0];

  is(entry.request.postData, undefined, "Check post data is not present");

  // Clean up
  return teardown(monitor);
});
