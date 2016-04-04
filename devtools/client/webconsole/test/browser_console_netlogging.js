/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network log messages bring up the network panel.

"use strict";

const TEST_NETWORK_REQUEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/" +
  "test-network-request.html";

add_task(function* () {
  let finishedRequest = waitForFinishedRequest(({ request }) => {
    return request.url === TEST_NETWORK_REQUEST_URI;
  });

  const hud = yield loadPageAndGetHud(TEST_NETWORK_REQUEST_URI,
                                      "browserConsole");
  let request = yield finishedRequest;

  ok(request, "Page load was logged");

  let client = hud.ui.webConsoleClient;
  let args = [request.actor];
  const postData = yield getPacket(client, "getRequestPostData", args);
  const responseContent = yield getPacket(client, "getResponseContent", args);

  is(request.request.url, TEST_NETWORK_REQUEST_URI,
    "Logged network entry is page load");
  is(request.request.method, "GET", "Method is correct");
  ok(!postData.postData.text, "No request body was stored");
  ok(postData.postDataDiscarded, "Request body was discarded");
  ok(!responseContent.content.text, "No response body was stored");
  ok(responseContent.contentDiscarded || request.fromCache,
     "Response body was discarded or response came from the cache");
});
