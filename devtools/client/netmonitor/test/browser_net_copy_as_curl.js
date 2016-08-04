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
      "-H 'x-custom-header-1: Custom value'",
      "-H 'x-custom-header-2: 8.8.8.8'",
      "-H 'x-custom-header-3: Mon, 3 Mar 2014 11:11:11 GMT'",
      "-H 'Referer: " + CURL_URL + "'",
      "-H 'Connection: keep-alive'",
      "-H 'Pragma: no-cache'",
      "-H 'Cache-Control: no-cache'"
    ].join(" ");

    const EXPECTED_WIN_RESULT = [
      "curl",
      '"' + SIMPLE_SJS + '"',
      '-H "Host: example.com"',
      '-H "User-Agent: ' + navigator.userAgent + '"',
      '-H "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"',
      '-H "Accept-Language: ' + navigator.language + '"',
      "--compressed",
      '-H "x-custom-header-1: Custom value"',
      '-H "x-custom-header-2: 8.8.8.8"',
      '-H "x-custom-header-3: Mon, 3 Mar 2014 11:11:11 GMT"',
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

      waitForClipboard(function validate(aResult) {
        if (typeof aResult !== "string") {
          return false;
        }

        // Different setups may produce the same command, but with the
        // parameters in a different order in the commandline (which is fine).
        // Here we confirm that the commands are the same even in that case.
        var expected = EXPECTED_RESULT.toString().match(/[-A-Za-z1-9]+ ([\"'])(?:\\\1|.)*?\1/g),
            actual = aResult.match(/[-A-Za-z1-9]+ ([\"'])(?:\\\1|.)*?\1/g);

        // Must begin with the same "curl 'URL'" segment
        if (!actual || expected[0] != actual[0]) {
          return false;
        }

        // Must match each of the params in the middle (headers)
        var expectedSet = new Set(expected);
        var actualSet = new Set(actual);
        if (expected.size != actual.size) {
          return false;
        }
        for (let param of expectedSet) {
          if (!actualSet.has(param)) {
            return false;
          }
        }

        return true;
      }, function setup() {
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

    function cleanUp() {
      teardown(aMonitor).then(finish);
    }
  });
}
