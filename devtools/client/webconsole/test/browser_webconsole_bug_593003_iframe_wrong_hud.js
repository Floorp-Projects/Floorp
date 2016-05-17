/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-593003-iframe-wrong-hud.html";

const TEST_IFRAME_URI = "http://example.com/browser/devtools/client/" +
                        "webconsole/test/test-bug-593003-iframe-wrong-" +
                        "hud-iframe.html";

const TEST_DUMMY_URI = "http://example.com/browser/devtools/client/" +
                       "webconsole/test/test-console.html";

add_task(function* () {

  let tab1 = (yield loadTab(TEST_URI)).tab;
  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.console.log("FOO");
  });
  yield openConsole();

  let tab2 = (yield loadTab(TEST_DUMMY_URI)).tab;
  yield openConsole(gBrowser.selectedTab);

  info("Reloading tab 1");
  yield reloadTab(tab1);

  info("Checking for messages");
  yield checkMessages(tab1, tab2);

  info("Cleaning up");
  yield closeConsole(tab1);
  yield closeConsole(tab2);
});

function* reloadTab(tab) {
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  yield loaded;
}

function* checkMessages(tab1, tab2) {
  let hud1 = yield openConsole(tab1);
  let outputNode1 = hud1.outputNode;

  info("Waiting for messages");
  yield waitForMessages({
    webconsole: hud1,
    messages: [{
      text: TEST_IFRAME_URI,
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }]
  });

  let hud2 = yield openConsole(tab2);
  let outputNode2 = hud2.outputNode;

  isnot(outputNode1, outputNode2,
    "the two HUD outputNodes must be different");

  let msg = "Didn't find the iframe network request in tab2";
  testLogEntry(outputNode2, TEST_IFRAME_URI, msg, true, true);
}
