/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Copy as cURL works.
 */

add_task(async function() {
  let { tab, monitor } = await initNetMonitor(CURL_URL);
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
  const BASE_RESULT = [
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

  const COOKIE_PARTIAL_RESULT = [
    header("Cookie: bob=true; tom=cool")
  ];

  const POST_PAYLOAD = "Plaintext value as a payload";
  const POST_PARTIAL_RESULT = [
    "--data '" + POST_PAYLOAD + "'",
    header("Content-Type: text/plain;charset=UTF-8")
  ];

  // GET request, no cookies (first request)
  await performRequest(null);
  await testClipboardContent(BASE_RESULT);

  // GET request, cookies set by previous response
  await performRequest(null);
  await testClipboardContent([
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT
  ]);

  // POST request
  await performRequest(POST_PAYLOAD);
  await testClipboardContent([
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT,
    ...POST_PARTIAL_RESULT
  ]);

  await teardown(monitor);

  async function performRequest(payload) {
    let wait = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, {
      url: SIMPLE_SJS, payload_: payload
    }, async function({url, payload_}) {
      content.wrappedJSObject.performRequest(url, payload_);
    });
    await wait;
  }

  async function testClipboardContent(expectedResult) {
    let { document } = monitor.panelWin;

    const items = document.querySelectorAll(".request-list-item");
    EventUtils.sendMouseEvent({ type: "mousedown" }, items[items.length - 1]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[0]);

    await waitForClipboardPromise(function setup() {
      monitor.panelWin.parent.document
        .querySelector("#request-list-context-copy-as-curl").click();
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
      if (!actual || expectedResult[0] != actual[0]) {
        return false;
      }

      // Must match each of the params in the middle (headers and --compressed)
      return expectedResult.length === actual.length &&
        expectedResult.every(param => actual.includes(param));
    });

    info("Clipboard contains a cURL command for the currently selected item's url.");
  }
});
