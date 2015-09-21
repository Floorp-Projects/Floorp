/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const INIT_URI = "data:text/html;charset=utf-8,Web Console - bug 600183 test";
const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-600183-charset.html";

function performTest(lastFinishedRequest, console) {
  let deferred = promise.defer();

  ok(lastFinishedRequest, "charset test page was loaded and logged");
  HUDService.lastFinishedRequest.callback = null;

  executeSoon(() => {
    console.webConsoleClient.getResponseContent(lastFinishedRequest.actor,
      (response) => {
        ok(!response.contentDiscarded, "response body was not discarded");

        let body = response.content.text;
        ok(body, "we have the response body");

        // 的问候!
        let chars = "\u7684\u95ee\u5019!";
        isnot(body.indexOf("<p>" + chars + "</p>"), -1,
          "found the chinese simplified string");

        HUDService.lastFinishedRequest.callback = null;
        executeSoon(deferred.resolve);
      });
  });

  return deferred.promise;
}

function waitForRequest() {
  let deferred = promise.defer();
  HUDService.lastFinishedRequest.callback = (req, console) => {
    performTest(req, console).then(deferred.resolve);
  };
  return deferred.promise;
}

var test = asyncTest(function* () {
  let { browser } = yield loadTab(INIT_URI);

  let hud = yield openConsole();

  yield hud.ui.setSaveRequestAndResponseBodies(true);

  ok(hud.ui._saveRequestAndResponseBodies,
    "The saveRequestAndResponseBodies property was successfully set.");

  let gotLastRequest = waitForRequest();

  let loaded = loadBrowser(browser);
  content.location = TEST_URI;
  yield loaded;

  yield gotLastRequest;
});
