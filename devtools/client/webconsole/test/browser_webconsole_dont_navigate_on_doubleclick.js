/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that if a link in console is double clicked, the console frame doesn't
// navigate to that destination (bug 975707).

"use strict";

function test() {
  let originalNetPref = Services.prefs
    .getBoolPref("devtools.webconsole.filter.networkinfo");
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo",
                               originalNetPref);
  });
  Services.prefs.setBoolPref("devtools.webconsole.filter.networkinfo", true);
  Task.spawn(runner).then(finishTest);

  function* runner() {
    const TEST_PAGE_URI = "http://example.com/browser/devtools/client/" +
                          "webconsole/test/test-console.html" + "?_uniq=" +
                          Date.now();

    const {tab} = yield loadTab("data:text/html;charset=utf8,<p>hello</p>");
    const hud = yield openConsole(tab);

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_PAGE_URI);

    let messages = yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "Network request message",
        url: TEST_PAGE_URI,
        category: CATEGORY_NETWORK
      }]
    });

    let networkEventMessage = messages[0].matched.values().next().value;
    let urlNode = networkEventMessage.querySelector(".url");

    let deferred = promise.defer();
    urlNode.addEventListener("click", function onClick(aEvent) {
      urlNode.removeEventListener("click", onClick);
      ok(aEvent.defaultPrevented, "The default action was prevented.");

      deferred.resolve();
    });

    EventUtils.synthesizeMouseAtCenter(urlNode, {clickCount: 2},
                                       hud.iframeWindow);

    yield deferred.promise;
  }
}
