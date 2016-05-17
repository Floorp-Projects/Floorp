/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "bug 630733";
const TEST_URI2 = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/test-bug-630733-response-redirect-headers.sjs";

var lastFinishedRequests = {};
var webConsoleClient;

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  yield consoleOpened(hud);
  yield getHeaders();
  yield getContent();

  performTest();
});

function consoleOpened(hud) {
  let deferred = promise.defer();

  webConsoleClient = hud.ui.webConsoleClient;
  HUDService.lastFinishedRequest.callback = (aHttpRequest) => {
    let status = aHttpRequest.response.status;
    lastFinishedRequests[status] = aHttpRequest;
    if ("301" in lastFinishedRequests &&
        "404" in lastFinishedRequests) {
      deferred.resolve();
    }
  };
  content.location = TEST_URI2;

  return deferred.promise;
}

function getHeaders() {
  let deferred = promise.defer();

  HUDService.lastFinishedRequest.callback = null;

  ok("301" in lastFinishedRequests, "request 1: 301 Moved Permanently");
  ok("404" in lastFinishedRequests, "request 2: 404 Not found");

  webConsoleClient.getResponseHeaders(lastFinishedRequests["301"].actor,
    function (response) {
      lastFinishedRequests["301"].response.headers = response.headers;

      webConsoleClient.getResponseHeaders(lastFinishedRequests["404"].actor,
        function (resp) {
          lastFinishedRequests["404"].response.headers = resp.headers;
          executeSoon(deferred.resolve);
        });
    });
  return deferred.promise;
}

function getContent() {
  let deferred = promise.defer();

  webConsoleClient.getResponseContent(lastFinishedRequests["301"].actor,
    function (response) {
      lastFinishedRequests["301"].response.content = response.content;
      lastFinishedRequests["301"].discardResponseBody = response.contentDiscarded;

      webConsoleClient.getResponseContent(lastFinishedRequests["404"].actor,
        function (resp) {
          lastFinishedRequests["404"].response.content = resp.content;
          lastFinishedRequests["404"].discardResponseBody =
            resp.contentDiscarded;

          webConsoleClient = null;
          executeSoon(deferred.resolve);
        });
    });
  return deferred.promise;
}

function performTest() {
  function readHeader(name) {
    for (let header of headers) {
      if (header.name == name) {
        return header.value;
      }
    }
    return null;
  }

  let headers = lastFinishedRequests["301"].response.headers;
  is(readHeader("Content-Type"), "text/html",
     "we do have the Content-Type header");
  is(readHeader("Content-Length"), 71, "Content-Length is correct");
  is(readHeader("Location"), "/redirect-from-bug-630733",
     "Content-Length is correct");
  is(readHeader("x-foobar-bug630733"), "bazbaz",
     "X-Foobar-bug630733 is correct");

  let body = lastFinishedRequests["301"].response.content;
  ok(!body.text, "body discarded for request 1");
  ok(lastFinishedRequests["301"].discardResponseBody,
     "body discarded for request 1 (confirmed)");

  headers = lastFinishedRequests["404"].response.headers;
  ok(!readHeader("Location"), "no Location header");
  ok(!readHeader("x-foobar-bug630733"), "no X-Foobar-bug630733 header");

  body = lastFinishedRequests["404"].response.content.text;
  isnot(body.indexOf("404"), -1,
        "body is correct for request 2");

  lastFinishedRequests = webConsoleClient = null;
}
