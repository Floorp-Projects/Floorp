/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-600183-charset.html";

let lastFinishedRequest = null;

function requestDoneCallback(aHttpRequest)
{
  lastFinishedRequest = aHttpRequest;
}

function performTest()
{
  ok(lastFinishedRequest, "charset test page was loaded and logged");

  let body = lastFinishedRequest.response.body;
  ok(body, "we have the response body");

  let chars = "\u7684\u95ee\u5019!"; // 的问候!
  isnot(body.indexOf("<p>" + chars + "</p>"), -1,
    "found the chinese simplified string");

  lastFinishedRequest = null;
  HUDService.saveRequestAndResponseBodies = false;
  HUDService.lastFinishedRequestCallback = null;
  finishTest();
}

function test()
{
  addTab("data:text/html;charset=utf-8,Web Console - bug 600183 test");

  let initialLoad = true;

  browser.addEventListener("load", function () {
    if (initialLoad) {
      waitForFocus(function() {
        openConsole();

        HUDService.saveRequestAndResponseBodies = true;
        HUDService.lastFinishedRequestCallback = requestDoneCallback;

        content.location = TEST_URI;
      }, content);
      initialLoad = false;
    } else {
      browser.removeEventListener("load", arguments.callee, true);
      performTest();
    }
  }, true);
}
