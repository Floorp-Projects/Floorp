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

function performTest(lastFinishedRequest)
{
  ok(lastFinishedRequest, "page load was logged");

  function readHeader(aName)
  {
    for (let header of headers) {
      if (header.name == aName) {
        return header.value;
      }
    }
    return null;
  }

  let headers = lastFinishedRequest.log.entries[0].response.headers;
  ok(headers, "we have the response headers");
  ok(!readHeader("Content-Type"), "we do not have the Content-Type header");
  isnot(readHeader("Content-Length"), 60, "Content-Length != 60");

  HUDService.lastFinishedRequestCallback = null;
  executeSoon(finishTest);
}

function test()
{
  addTab(TEST_URI);

  let initialLoad = true;

  browser.addEventListener("load", function onLoad() {
    if (initialLoad) {
      openConsole(null, function() {
        HUDService.lastFinishedRequestCallback = performTest;
        content.location.reload();
      });
      initialLoad = false;
    } else {
      browser.removeEventListener("load", onLoad, true);
    }
  }, true);
}
