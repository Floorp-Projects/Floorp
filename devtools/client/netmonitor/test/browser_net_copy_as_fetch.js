/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if Copy as Fetch works.
 */

add_task(async function() {
  const { tab, monitor } = await initNetMonitor(CURL_URL);
  info("Starting test... ");

  // GET request, no cookies (first request)
  await performRequest("GET");
  await testClipboardContent(`await fetch("http://example.com/browser/devtools/client/netmonitor/test/sjs_simple-test-server.sjs", {
    "credentials": "omit",
    "headers": {
        "User-Agent": "${navigator.userAgent}",
        "Accept": "*/*",
        "Accept-Language": "en-US",
        "X-Custom-Header-1": "Custom value",
        "X-Custom-Header-2": "8.8.8.8",
        "X-Custom-Header-3": "Mon, 3 Mar 2014 11:11:11 GMT",
        "Pragma": "no-cache",
        "Cache-Control": "no-cache"
    },
    "referrer": "http://example.com/browser/devtools/client/netmonitor/test/html_copy-as-curl.html",
    "method": "GET",
    "mode": "cors"
});`);

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

    /* Ensure that the copy as fetch option is always visible */
    const copyAsFetchNode = getContextMenuItem(monitor,
      "request-list-context-copy-as-fetch");
    is(!!copyAsFetchNode, true,
      "The \"Copy as Fetch\" context menu item should not be hidden.");

    await waitForClipboardPromise(function setup() {
      copyAsFetchNode.click();
    }, function validate(result) {
      if (typeof result !== "string") {
        return false;
      }

      return expectedResult === result;
    });

    info("Clipboard contains a fetch command for item " + (items.length - 1));
  }
});
