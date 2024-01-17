/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Copy as Fetch works.
 */

add_task(async function () {
  const { tab, monitor } = await initNetMonitor(HTTPS_CURL_URL, {
    requestCount: 1,
  });
  info("Starting test... ");

  // GET request, no cookies (first request)
  await performRequest("GET");
  await testClipboardContent(`await fetch("https://example.com/browser/devtools/client/netmonitor/test/sjs_simple-test-server.sjs", {
    "credentials": "omit",
    "headers": {
        "Accept": "*/*",
        "Accept-Language": "en-US",
        "X-Custom-Header-1": "Custom value",
        "X-Custom-Header-2": "8.8.8.8",
        "X-Custom-Header-3": "Mon, 3 Mar 2014 11:11:11 GMT",
        "User-Agent": "${navigator.userAgent}",
        "Sec-Fetch-Dest": "empty",
        "Sec-Fetch-Mode": "cors",
        "Sec-Fetch-Site": "same-origin",
        "Pragma": "no-cache",
        "Cache-Control": "no-cache"
    },
    "referrer": "https://example.com/browser/devtools/client/netmonitor/test/html_copy-as-curl.html",
    "method": "GET",
    "mode": "cors"
});`);

  await teardown(monitor);

  async function performRequest(method, payload) {
    const waitRequest = waitForNetworkEvents(monitor, 1);
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [
        {
          url: HTTPS_SIMPLE_SJS,
          method_: method,
          payload_: payload,
        },
      ],
      async function ({ url, method_, payload_ }) {
        content.wrappedJSObject.performRequest(url, method_, payload_);
      }
    );
    await waitRequest;
  }

  async function testClipboardContent(expectedResult) {
    const { document } = monitor.panelWin;

    const items = document.querySelectorAll(".request-list-item");
    EventUtils.sendMouseEvent({ type: "mousedown" }, items[items.length - 1]);
    EventUtils.sendMouseEvent(
      { type: "contextmenu" },
      document.querySelectorAll(".request-list-item")[0]
    );

    /* Ensure that the copy as fetch option is always visible */
    is(
      !!getContextMenuItem(monitor, "request-list-context-copy-as-fetch"),
      true,
      'The "Copy as Fetch" context menu item should not be hidden.'
    );

    await waitForClipboardPromise(
      async function setup() {
        await selectContextMenuItem(
          monitor,
          "request-list-context-copy-as-fetch"
        );
      },
      function validate(result) {
        if (typeof result !== "string") {
          return false;
        }
        return expectedResult === result;
      }
    );

    info("Clipboard contains a fetch command for item " + (items.length - 1));
  }
});
