/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-630733-response-redirect-headers.sjs";

let lastFinishedRequests = {};

function requestDoneCallback(aHttpRequest)
{
  let status = aHttpRequest.log.entries[0].response.status;
  lastFinishedRequests[status] = aHttpRequest;
}

function performTest(aEvent)
{
  HUDService.lastFinishedRequestCallback = null;

  ok("301" in lastFinishedRequests, "request 1: 301 Moved Permanently");
  ok("404" in lastFinishedRequests, "request 2: 404 Not found");

  function readHeader(aName)
  {
    for (let header of headers) {
      if (header.name == aName) {
        return header.value;
      }
    }
    return null;
  }

  let headers = lastFinishedRequests["301"].log.entries[0].response.headers;
  is(readHeader("Content-Type"), "text/html",
     "we do have the Content-Type header");
  is(readHeader("Content-Length"), 71, "Content-Length is correct");
  is(readHeader("Location"), "/redirect-from-bug-630733",
     "Content-Length is correct");
  is(readHeader("x-foobar-bug630733"), "bazbaz",
     "X-Foobar-bug630733 is correct");
  let body = lastFinishedRequests["301"].log.entries[0].response.content;
  ok(!body.text, "body discarded for request 1");

  headers = lastFinishedRequests["404"].log.entries[0].response.headers;
  ok(!readHeader("Location"), "no Location header");
  ok(!readHeader("x-foobar-bug630733"), "no X-Foobar-bug630733 header");

  body = lastFinishedRequests["404"].log.entries[0].response.content.text;
  isnot(body.indexOf("404"), -1,
        "body is correct for request 2");

  lastFinishedRequests = null;
  executeSoon(finishTest);
}

function test()
{
  addTab("data:text/html;charset=utf-8,<p>Web Console test for bug 630733");

  browser.addEventListener("load", function onLoad1(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad1, true);

    openConsole(null, function(hud) {
      hud.ui.saveRequestAndResponseBodies = true;
      HUDService.lastFinishedRequestCallback = requestDoneCallback;

      browser.addEventListener("load", function onLoad2(aEvent) {
        browser.removeEventListener(aEvent.type, onLoad2, true);
        executeSoon(performTest);
      }, true);

      content.location = TEST_URI;
    });
  }, true);
}
