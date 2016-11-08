/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Copy as cURL works.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CURL_URL);
  info("Starting test... ");

  // Different quote chars are used for Windows and POSIX
  const QUOTE = Services.appinfo.OS == "WINNT" ? "\"" : "'";

  // Quote a string, escape the quotes inside the string
  function quote(str) {
    return QUOTE + str.replace(new RegExp(QUOTE, "g"), `\\${QUOTE}`) + QUOTE;
  }

  // Header param is formatted as -H "Header: value" or -H 'Header: value'
  function header(h) {
    return "-H " + quote(h);
  }

  // Construct the expected command
  const EXPECTED_RESULT = [
    "curl " + quote(SIMPLE_SJS),
    "--compressed",
    header("Host: example.com"),
    header("User-Agent: " + navigator.userAgent),
    header("Accept: */*"),
    header("Accept-Language: " + navigator.language),
    header("X-Custom-Header-1: Custom value"),
    header("X-Custom-Header-2: 8.8.8.8"),
    header("X-Custom-Header-3: Mon, 3 Mar 2014 11:11:11 GMT"),
    header("Referer: " + CURL_URL),
    header("Connection: keep-alive"),
    header("Pragma: no-cache"),
    header("Cache-Control: no-cache")
  ];

  let { NetMonitorView } = monitor.panelWin;
  let { RequestsMenu } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, SIMPLE_SJS, function* (url) {
    content.wrappedJSObject.performRequest(url);
  });
  yield wait;

  let requestItem = RequestsMenu.getItemAtIndex(0);
  RequestsMenu.selectedItem = requestItem;

  yield waitForClipboardPromise(function setup() {
    RequestsMenu.contextMenu.copyAsCurl();
  }, function validate(result) {
    if (typeof result !== "string") {
      return false;
    }

    // Different setups may produce the same command, but with the
    // parameters in a different order in the commandline (which is fine).
    // Here we confirm that the commands are the same even in that case.

    // This monster regexp parses the command line into an array of arguments,
    // recognizing quoted args with matching quotes and escaped quotes inside:
    // [ "curl 'url'", "--standalone-arg", "-arg-with-quoted-string 'value\'s'" ]
    let matchRe = /[-A-Za-z1-9]+(?: ([\"'])(?:\\\1|.)*?\1)?/g;

    let actual = result.match(matchRe);

    // Must begin with the same "curl 'URL'" segment
    if (!actual || EXPECTED_RESULT[0] != actual[0]) {
      return false;
    }

    // Must match each of the params in the middle (headers and --compressed)
    return EXPECTED_RESULT.length === actual.length &&
      EXPECTED_RESULT.every(param => actual.includes(param));
  });

  info("Clipboard contains a cURL command for the currently selected item's url.");

  yield teardown(monitor);
});
