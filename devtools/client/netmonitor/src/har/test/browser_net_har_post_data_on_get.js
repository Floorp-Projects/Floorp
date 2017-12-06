/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for exporting POST data into HAR format.
 */
add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(
    HAR_EXAMPLE_URL + "html_har_post-data-test-page.html");

  info("Starting test... ");

  let { connector, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  let RequestListContextMenu = windowRequire(
    "devtools/client/netmonitor/src/widgets/RequestListContextMenu");
  let { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  // Execute one GET request on the page and wait till its done.
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, {}, function* () {
    content.wrappedJSObject.executeTest3();
  });
  yield wait;

  // Copy HAR into the clipboard (asynchronous).
  let contextMenu = new RequestListContextMenu({ connector });
  let jsonString = yield contextMenu.copyAllAsHar(getSortedRequests(store.getState()));
  let har = JSON.parse(jsonString);

  // Check out the HAR log.
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  let entry = har.log.entries[0];

  is(entry.request.postData, undefined, "Check post data is not present");

  // Clean up
  return teardown(monitor);
});
