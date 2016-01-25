/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const INIT_URI = "data:text/plain;charset=utf8,hello world";
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-599725-response-headers.sjs";

var loads = 0;
function performTest(request, console) {
  let deferred = promise.defer();

  let headers = null;

  function readHeader(name) {
    for (let header of headers) {
      if (header.name == name) {
        return header.value;
      }
    }
    return null;
  }

  console.webConsoleClient.getResponseHeaders(request.actor,
    function(response) {
      headers = response.headers;
      ok(headers, "we have the response headers for reload");

      let contentType = readHeader("Content-Type");
      let contentLength = readHeader("Content-Length");

      ok(!contentType, "we do not have the Content-Type header");
      isnot(contentLength, 60, "Content-Length != 60");

      if (contentType || contentLength == 60) {
        console.debug("lastFinishedRequest", lastFinishedRequest,
                      "request", lastFinishedRequest.request,
                      "response", lastFinishedRequest.response,
                      "updates", lastFinishedRequest.updates,
                      "response headers", headers);
      }

      executeSoon(deferred.resolve);
    });

  HUDService.lastFinishedRequest.callback = null;

  return deferred.promise;
}

function waitForRequest() {
  let deferred = promise.defer();
  HUDService.lastFinishedRequest.callback = (req, console) => {
    loads++;
    ok(req, "page load was logged");
    if (loads != 2) {
      return;
    }
    performTest(req, console).then(deferred.resolve);
  };
  return deferred.promise;
}

add_task(function* () {
  let { browser } = yield loadTab(INIT_URI);

  yield openConsole();

  let gotLastRequest = waitForRequest();

  let loaded = loadBrowser(browser);
  content.location = TEST_URI;
  yield loaded;

  let reloaded = loadBrowser(browser);
  content.location.reload();
  yield reloaded;

  yield gotLastRequest;
});
