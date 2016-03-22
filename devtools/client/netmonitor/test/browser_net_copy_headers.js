/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if copying a request's request/response headers works.
 */

add_task(function*() {

  let [ aTab, aDebuggee, aMonitor ] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { NetMonitorView } = aMonitor.panelWin;
  let { RequestsMenu } = NetMonitorView;

  RequestsMenu.lazyUpdate = false;

  aDebuggee.location.reload();
  yield waitForNetworkEvents(aMonitor, 1);

  let requestItem = RequestsMenu.getItemAtIndex(0);
  RequestsMenu.selectedItem = requestItem;

  let clipboard = null;

  const EXPECTED_REQUEST_HEADERS = [
    requestItem.attachment.method + " " + SIMPLE_URL + " " + requestItem.attachment.httpVersion,
    "Host: example.com",
    "User-Agent: " + navigator.userAgent + "",
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    "Accept-Language: " + navigator.languages.join(",") + ";q=0.5",
    "Accept-Encoding: gzip, deflate",
    "Connection: keep-alive",
    "Upgrade-Insecure-Requests: 1",
    "Pragma: no-cache",
    "Cache-Control: no-cache"
  ].join("\n");

  RequestsMenu.copyRequestHeaders();
  clipboard = SpecialPowers.getClipboardData("text/unicode");
  // Sometimes, a "Cookie" header is left over from other tests. Remove it:
  clipboard = clipboard.replace(/Cookie: [^\n]+\n/, "");
  is(clipboard, EXPECTED_REQUEST_HEADERS, "Clipboard contains the currently selected item's request headers.");

  const EXPECTED_RESPONSE_HEADERS = [
    requestItem.attachment.httpVersion + " " + requestItem.attachment.status + " " + requestItem.attachment.statusText,
    "Last-Modified: Sun, 3 May 2015 11:11:11 GMT",
    "Content-Type: text/html",
    "Content-Length: 465",
    "Connection: close",
    "Server: httpd.js",
    "Date: Sun, 3 May 2015 11:11:11 GMT"
  ].join("\n");

  RequestsMenu.copyResponseHeaders();
  clipboard = SpecialPowers.getClipboardData("text/unicode");
  // Fake the "Last-Modified" and "Date" headers because they will vary:
  clipboard = clipboard
    .replace(/Last-Modified: [^\n]+ GMT/, "Last-Modified: Sun, 3 May 2015 11:11:11 GMT")
    .replace(/Date: [^\n]+ GMT/, "Date: Sun, 3 May 2015 11:11:11 GMT");
  is(clipboard, EXPECTED_RESPONSE_HEADERS, "Clipboard contains the currently selected item's response headers.");

  teardown(aMonitor).then(finish);
});
