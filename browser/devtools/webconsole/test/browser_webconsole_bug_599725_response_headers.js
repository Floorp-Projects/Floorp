/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-599725-response-headers.sjs";

let loads = 0;
function performTest(aRequest, aConsole)
{
  loads++;
  ok(aRequest, "page load was logged");
  if (loads != 2) {
    return;
  }

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

      executeSoon(finishTest);
    });

  HUDService.lastFinishedRequestCallback = null;
}

function test()
{
  addTab("data:text/plain;charset=utf8,hello world");

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, () => {
      HUDService.lastFinishedRequestCallback = performTest;

      browser.addEventListener("load", function onReload() {
        browser.removeEventListener("load", onReload, true);
        executeSoon(() => content.location.reload());
      }, true);

      executeSoon(() => content.location = TEST_URI);
    });
  }, true);
}
