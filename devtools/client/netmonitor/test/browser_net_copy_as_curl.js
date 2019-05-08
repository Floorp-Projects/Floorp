/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Copy as cURL works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CURL_URL);
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
  const SIMPLE_BASE = [
    "curl " + quote(SIMPLE_SJS),
  ];
  const SLOW_BASE = [
    "curl " + quote(SLOW_SJS),
  ];
  const BASE_RESULT = [
    "--compressed",
    header("User-Agent: " + navigator.userAgent),
    header("Accept: */*"),
    header("Accept-Language: " + navigator.language),
    header("X-Custom-Header-1: Custom value"),
    header("X-Custom-Header-2: 8.8.8.8"),
    header("X-Custom-Header-3: Mon, 3 Mar 2014 11:11:11 GMT"),
    header("Referer: " + CURL_URL),
    header("Connection: keep-alive"),
    header("Pragma: no-cache"),
    header("Cache-Control: no-cache"),
  ];

  const COOKIE_PARTIAL_RESULT = [
    header("Cookie: bob=true; tom=cool"),
  ];

  const POST_PAYLOAD = "Plaintext value as a payload";
  const POST_PARTIAL_RESULT = [
    "--data " + quote(POST_PAYLOAD),
    header("Content-Type: text/plain;charset=UTF-8"),
  ];

  const HEAD_PARTIAL_RESULT = [
    "-I",
  ];

  // GET request, no cookies (first request)
  await performRequest("GET");
  await testClipboardContent([
    ...SIMPLE_BASE,
    ...BASE_RESULT,
  ]);
  // Check to make sure it is still OK after we view the response (bug#1452442)
  await selectIndexAndWaitForSourceEditor(monitor, 0);
  await testClipboardContent([
    ...SIMPLE_BASE,
    ...BASE_RESULT,
  ]);

  // GET request, cookies set by previous response
  await performRequest("GET");
  await testClipboardContent([
    ...SIMPLE_BASE,
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT,
  ]);

  // Unfinished request (bug#1378464, bug#1420513)
  const waitSlow = waitForNetworkEvents(monitor, 0);
  await ContentTask.spawn(tab.linkedBrowser, SLOW_SJS, async function(url) {
    content.wrappedJSObject.performRequest(url, "GET", null);
  });
  await waitSlow;
  await testClipboardContent([
    ...SLOW_BASE,
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT,
  ]);

  // POST request
  await performRequest("POST", POST_PAYLOAD);
  await testClipboardContent([
    ...SIMPLE_BASE,
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT,
    ...POST_PARTIAL_RESULT,
  ]);

  // HEAD request
  await performRequest("HEAD");
  await testClipboardContent([
    ...SIMPLE_BASE,
    ...BASE_RESULT,
    ...COOKIE_PARTIAL_RESULT,
    ...HEAD_PARTIAL_RESULT,
  ]);

  await teardown(monitor);

  async function performRequest(method, payload) {
    const waitRequest = waitForNetworkEvents(monitor, 1);
    await ContentTask.spawn(tab.linkedBrowser, {
      url: SIMPLE_SJS,
      method_: method,
      payload_: payload,
    }, async function({url, method_, payload_}) {
      content.wrappedJSObject.performRequest(url, method_, payload_);
    });
    await waitRequest;
  }

  async function testClipboardContent(expectedResult) {
    const { document } = monitor.panelWin;

    const items = document.querySelectorAll(".request-list-item");
    EventUtils.sendMouseEvent({ type: "mousedown" }, items[items.length - 1]);
    EventUtils.sendMouseEvent({ type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[0]);

    /* Ensure that the copy as cURL option is always visible */
    const copyUrlParamsNode = getContextMenuItem(monitor,
      "request-list-context-copy-as-curl");
    is(!!copyUrlParamsNode, true,
      "The \"Copy as cURL\" context menu item should not be hidden.");

    await waitForClipboardPromise(function setup() {
      copyUrlParamsNode.click();
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
      const matchRe = /[-A-Za-z1-9]+(?: ([\"'])(?:\\\1|.)*?\1)?/g;

      const actual = result.match(matchRe);
      // Must begin with the same "curl 'URL'" segment
      if (!actual || expectedResult[0] != actual[0]) {
        return false;
      }

      // Must match each of the params in the middle (headers and --compressed)
      return expectedResult.length === actual.length &&
        expectedResult.every(param => actual.includes(param));
    });

    info("Clipboard contains a cURL command for item " + (items.length - 1));
  }
});
