/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const INIT_URI = "data:text/plain;charset=utf8,hello world";
const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-599725-response-headers.sjs";

let loads = 0;
function performTest(aRequest, aConsole)
{
  let deferred = promise.defer();

  let headers = null;

  function readHeader(aName)
  {
    for (let header of headers) {
      if (header.name == aName) {
        return header.value;
      }
    }
    return null;
  }

  aConsole.webConsoleClient.getResponseHeaders(aRequest.actor,
    function (aResponse) {
      headers = aResponse.headers;
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

let test = asyncTest(function* () {
  let { browser } = yield loadTab(INIT_URI);

  let hud = yield openConsole();

  let gotLastRequest = waitForRequest();

  let loaded = loadBrowser(browser);
  content.location = TEST_URI;
  yield loaded;

  let reloaded = loadBrowser(browser);
  content.location.reload();
  yield reloaded;

  yield gotLastRequest;
});
