/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Basic tests for exporting Network panel content into HAR format.
 */
add_task(async function() {
  // Disable tcp fast open, because it is setting a response header indicator
  // (bug 1352274). TCP Fast Open is not present on all platforms therefore the
  // number of response headers will vary depending on the platform.
  await pushPref("network.tcp.tcp_fastopen_enable", false);
  const { tab, monitor } = await initNetMonitor(SIMPLE_URL);

  info("Starting test... ");

  let har = await reloadAndCopyAllAsHar(tab, monitor);

  // Check out HAR log
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.creator.name, "Firefox", "The creator field must be set");
  is(har.log.browser.name, "Firefox", "The browser field must be set");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  const page = har.log.pages[0];
  ok("onContentLoad" in page.pageTimings, "There must be onContentLoad time");
  ok("onLoad" in page.pageTimings, "There must be onLoad time");

  let entry = har.log.entries[0];
  ok(entry.time > 0, "Check the total time");
  is(entry.request.method, "GET", "Check the method");
  is(entry.request.url, SIMPLE_URL, "Check the URL");
  is(entry.request.headers.length, 9, "Check number of request headers");
  is(entry.response.status, 200, "Check response status");
  is(entry.response.statusText, "OK", "Check response status text");
  is(entry.response.headers.length, 6, "Check number of response headers");
  is(entry.response.content.mimeType, // eslint-disable-line
    "text/html", "Check response content type"); // eslint-disable-line
  isnot(entry.response.content.text, undefined, // eslint-disable-line
    "Check response body");
  isnot(entry.timings, undefined, "Check timings");

  // Test response body limit (non zero).
  await pushPref("devtools.netmonitor.responseBodyLimit", 10);
  har = await reloadAndCopyAllAsHar(tab, monitor);
  entry = har.log.entries[0];
  is(entry.response.content.text.length, 10, // eslint-disable-line
    "Response body must be truncated");

  // Test response body limit (zero).
  await pushPref("devtools.netmonitor.responseBodyLimit", 0);
  har = await reloadAndCopyAllAsHar(tab, monitor);
  entry = har.log.entries[0];
  is(entry.response.content.text.length, 465, // eslint-disable-line
    "Response body must not be truncated");

  return teardown(monitor);
});

/**
 * Reload the page and copy all as HAR.
 */
async function reloadAndCopyAllAsHar(tab, monitor) {
  const { connector, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { HarMenuUtils } = windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils");
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index");

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  await HarMenuUtils.copyAllAsHar(getSortedRequests(store.getState()), connector);

  const jsonString = SpecialPowers.getClipboardData("text/unicode");
  return JSON.parse(jsonString);
}
