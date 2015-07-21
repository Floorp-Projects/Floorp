/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if Copy as cURL works.
 */

function test() {
  initNetMonitor(CURL_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    const EXPECTED_POSIX_RESULT = [
      "curl",
      "'" + SIMPLE_SJS + "'",
      "-H 'Host: example.com'",
      "-H 'User-Agent: " + navigator.userAgent + "'",
      "-H 'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8'",
      "-H 'Accept-Language: " + navigator.language + "'",
      "--compressed",
      "-H 'X-Custom-Header-1: Custom value'",
      "-H 'X-Custom-Header-2: 8.8.8.8'",
      "-H 'X-Custom-Header-3: Mon, 3 Mar 2014 11:11:11 GMT'",
      "-H 'Referer: " + CURL_URL + "'",
      "-H 'Connection: keep-alive'",
      "-H 'Pragma: no-cache'",
      "-H 'Cache-Control: no-cache'"
    ].join(" ");

    const EXPECTED_WIN_RESULT = [
      'curl',
      '"' + SIMPLE_SJS + '"',
      '-H "Host: example.com"',
      '-H "User-Agent: ' + navigator.userAgent + '"',
      '-H "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"',
      '-H "Accept-Language: ' + navigator.language + '"',
      "--compressed",
      '-H "X-Custom-Header-1: Custom value"',
      '-H "X-Custom-Header-2: 8.8.8.8"',
      '-H "X-Custom-Header-3: Mon, 3 Mar 2014 11:11:11 GMT"',
      '-H "Referer: ' + CURL_URL + '"',
      '-H "Connection: keep-alive"',
      '-H "Pragma: no-cache"', 
      '-H "Cache-Control: no-cache"'
    ].join(" ");

    const EXPECTED_RESULT = Services.appinfo.OS == "WINNT"
      ? EXPECTED_WIN_RESULT
      : EXPECTED_POSIX_RESULT;

    let { NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 1).then(() => {
      let requestItem = RequestsMenu.getItemAtIndex(0);
      RequestsMenu.selectedItem = requestItem;

      waitForClipboard(EXPECTED_RESULT, function setup() {
        RequestsMenu.copyAsCurl();
      }, function onSuccess() {
        ok(true, "Clipboard contains a cURL command for the currently selected item's url.");
        cleanUp();
      }, function onFailure() {
        ok(false, "Creating a cURL command for the currently selected item was unsuccessful.");
        cleanUp();
      });

    });

    aDebuggee.performRequest(SIMPLE_SJS);

    function cleanUp(){
      teardown(aMonitor).then(finish);
    }
  });
}
