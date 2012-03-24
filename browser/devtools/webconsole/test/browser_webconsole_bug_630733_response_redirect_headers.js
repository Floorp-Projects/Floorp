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
  let status = aHttpRequest.response.status.
               replace(/^HTTP\/\d\.\d (\d+).+$/, "$1");
  lastFinishedRequests[status] = aHttpRequest;
}

function performTest(aEvent)
{
  HUDService.saveRequestAndResponseBodies = false;
  HUDService.lastFinishedRequestCallback = null;

  ok("301" in lastFinishedRequests, "request 1: 301 Moved Permanently");
  ok("404" in lastFinishedRequests, "request 2: 404 Not found");

  let headers0 = lastFinishedRequests["301"].response.header;
  is(headers0["Content-Type"], "text/html",
     "we do have the Content-Type header");
  is(headers0["Content-Length"], 71, "Content-Length is correct");
  is(headers0["Location"], "/redirect-from-bug-630733",
     "Content-Length is correct");
  is(headers0["x-foobar-bug630733"], "bazbaz",
     "X-Foobar-bug630733 is correct");
  let body = lastFinishedRequests["301"].response.body;
  ok(!body, "body discarded for request 1");

  let headers1 = lastFinishedRequests["404"].response.header;
  ok(!headers1["Location"], "no Location header");
  ok(!headers1["x-foobar-bug630733"], "no X-Foobar-bug630733 header");

  body = lastFinishedRequests["404"].response.body;
  isnot(body.indexOf("404"), -1,
        "body is correct for request 2");

  lastFinishedRequests = null;
  finishTest();
}

function test()
{
  addTab("data:text/html;charset=utf-8,<p>Web Console test for bug 630733");

  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    executeSoon(function() {
      openConsole();

      HUDService.saveRequestAndResponseBodies = true;
      HUDService.lastFinishedRequestCallback = requestDoneCallback;

      browser.addEventListener("load", function(aEvent) {
        browser.removeEventListener(aEvent.type, arguments.callee, true);
        executeSoon(performTest);
      }, true);

      content.location = TEST_URI;
    });
  }, true);
}
