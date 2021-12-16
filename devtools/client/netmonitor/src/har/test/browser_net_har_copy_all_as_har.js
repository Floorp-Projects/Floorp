/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Basic tests for exporting Network panel content into HAR format.
 */

const EXPECTED_REQUEST_HEADER_COUNT = 9;
const EXPECTED_RESPONSE_HEADER_COUNT = 6;

add_task(async function() {
  // Disable tcp fast open, because it is setting a response header indicator
  // (bug 1352274). TCP Fast Open is not present on all platforms therefore the
  // number of response headers will vary depending on the platform.
  await pushPref("network.tcp.tcp_fastopen_enable", false);
  const { tab, monitor, toolbox } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  info("Starting test... ");

  let har = await reloadAndCopyAllAsHar({ tab, monitor, toolbox });

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
  assertNavigationRequestEntry(entry);
  isnot(entry.response.content.text, undefined, "Check response body");
  isnot(entry.timings, undefined, "Check timings");

  info("Test response body limit (non zero).");
  await pushPref("devtools.netmonitor.responseBodyLimit", 10);
  har = await reloadAndCopyAllAsHar({ tab, monitor, toolbox });
  entry = har.log.entries[0];
  is(entry.response.content.text.length, 10, "Response body must be truncated");

  info("Test response body limit (zero).");
  await pushPref("devtools.netmonitor.responseBodyLimit", 0);
  har = await reloadAndCopyAllAsHar({ tab, monitor, toolbox });
  entry = har.log.entries[0];
  is(
    entry.response.content.text.length,
    465,
    "Response body must not be truncated"
  );

  har = await reloadAndCopyAllAsHar({
    tab,
    monitor,
    toolbox,
    reloadTwice: true,
  });
  is(har.log.entries.length, 2, "There must be two requests");
  info(
    "Assert the first navigation request which has been cancelled by the second reload"
  );
  // Requests may come out of order, so try to find the bogus cancelled request
  entry = har.log.entries.find(e => e.response.status == 0);
  ok(entry, "Found the cancelled request");
  is(entry.request.method, "GET", "Method is set");
  is(entry.request.url, SIMPLE_URL, "URL is set");
  // We always get the following headers:
  // "Host", "User-agent", "Accept", "Accept-Language", "Accept-Encoding", "Connection"
  // but are missing the three last headers:
  // "Upgrade-Insecure-Requests", "Pragma", "Cache-Control"
  is(entry.request.headers.length, 6, "But headers are partialy populated");
  is(entry.response.status, 0, "And status is set to 0");

  entry = har.log.entries.find(e => e.response.status != 0);
  assertNavigationRequestEntry(entry);

  return teardown(monitor);
});

function assertNavigationRequestEntry(entry) {
  info("Assert that the entry relates to the navigation request");
  ok(entry.time > 0, "Check the total time");
  is(entry.request.method, "GET", "Check the method");
  is(entry.request.url, SIMPLE_URL, "Check the URL");
  is(
    entry.request.headers.length,
    EXPECTED_REQUEST_HEADER_COUNT,
    "Check number of request headers"
  );
  is(entry.response.status, 200, "Check response status");
  is(entry.response.statusText, "OK", "Check response status text");
  is(
    entry.response.headers.length,
    EXPECTED_RESPONSE_HEADER_COUNT,
    "Check number of response headers"
  );
  is(
    entry.response.content.mimeType,
    "text/html",
    "Check response content type"
  );
}
/**
 * Reload the page and copy all as HAR.
 */
async function reloadAndCopyAllAsHar({
  tab,
  monitor,
  toolbox,
  reloadTwice = false,
}) {
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

  if (reloadTwice) {
    reloadBrowser();
  }
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
