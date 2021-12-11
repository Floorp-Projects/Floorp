/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Basic tests for exporting Network panel content into HAR format.
 */
add_task(async function() {
  // Using https-first for this test is blocked on Bug 1733420.
  // Otherwise in https we receive the wrong headers length (12 instead of 9)
  // and the wrong statusText (Connected instead of OK).
  await pushPref("dom.security.https_first", false);

  // Disable tcp fast open, because it is setting a response header indicator
  // (bug 1352274). TCP Fast Open is not present on all platforms therefore the
  // number of response headers will vary depending on the platform.
  await pushPref("network.tcp.tcp_fastopen_enable", false);
  const { tab, monitor, toolbox } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  let har = await reloadAndCopyAllAsHar(tab, monitor, toolbox);

  // Check out HAR log
  isnot(har.log, null, "The HAR log must exist");
  is(har.log.creator.name, "Firefox", "The creator field must be set");
  is(har.log.browser.name, "Firefox", "The browser field must be set");
  is(har.log.pages.length, 1, "There must be one page");
  is(har.log.entries.length, 1, "There must be one request");

  const page = har.log.pages[0];

  is(page.title, "Network Monitor test page", "There must be some page title");
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
  is(
    entry.response.content.mimeType,
    "text/html",
    "Check response content type"
  );
  isnot(entry.response.content.text, undefined, "Check response body");
  isnot(entry.timings, undefined, "Check timings");

  // Test response body limit (non zero).
  await pushPref("devtools.netmonitor.responseBodyLimit", 10);
  har = await reloadAndCopyAllAsHar(tab, monitor, toolbox);
  entry = har.log.entries[0];
  is(entry.response.content.text.length, 10, "Response body must be truncated");

  // Test response body limit (zero).
  await pushPref("devtools.netmonitor.responseBodyLimit", 0);
  har = await reloadAndCopyAllAsHar(tab, monitor, toolbox);
  entry = har.log.entries[0];
  is(
    entry.response.content.text.length,
    465,
    "Response body must not be truncated"
  );

  return teardown(monitor);
});

/**
 * Reload the page and copy all as HAR.
 */
async function reloadAndCopyAllAsHar(tab, monitor, toolbox) {
  const { connector, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");
  const { HarMenuUtils } = windowRequire(
    "devtools/client/netmonitor/src/har/har-menu-utils"
  );
  const { getSortedRequests } = windowRequire(
    "devtools/client/netmonitor/src/selectors/index"
  );

  store.dispatch(Actions.batchEnable(false));

  const wait = waitForNetworkEvents(monitor, 1);
  const {
    onDomCompleteResource,
  } = await waitForNextTopLevelDomCompleteResource(toolbox.commands);

  await reloadBrowser();

  info("Waiting for network events");
  await wait;
  info("Waiting for DOCUMENT_EVENT dom-complete resource");
  await onDomCompleteResource;

  await HarMenuUtils.copyAllAsHar(
    getSortedRequests(store.getState()),
    connector
  );

  const jsonString = SpecialPowers.getClipboardData("text/unicode");
  return JSON.parse(jsonString);
}
