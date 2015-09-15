/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
                 "test/test-bug-593003-iframe-wrong-hud.html";

const TEST_IFRAME_URI = "http://example.com/browser/browser/devtools/" +
                        "webconsole/test/test-bug-593003-iframe-wrong-" +
                        "hud-iframe.html";

const TEST_DUMMY_URI = "http://example.com/browser/browser/devtools/" +
                       "webconsole/test/test-console.html";

var tab1, tab2;

function test() {
  loadTab(TEST_URI).then(({tab}) => {
    tab1 = tab;

    content.console.log("FOO");
    openConsole().then(() => {
      tab2 = gBrowser.addTab(TEST_DUMMY_URI);
      gBrowser.selectedTab = tab2;
      gBrowser.selectedBrowser.addEventListener("load", tab2Loaded, true);
    });
  });
}

function tab2Loaded(aEvent) {
  tab2.linkedBrowser.removeEventListener(aEvent.type, tab2Loaded, true);

  openConsole(gBrowser.selectedTab).then(() => {
    tab1.linkedBrowser.addEventListener("load", tab1Reloaded, true);
    tab1.linkedBrowser.contentWindow.location.reload();
  });
}

function tab1Reloaded(aEvent) {
  tab1.linkedBrowser.removeEventListener(aEvent.type, tab1Reloaded, true);

  let hud1 = HUDService.getHudByWindow(tab1.linkedBrowser.contentWindow);
  let outputNode1 = hud1.outputNode;

  waitForMessages({
    webconsole: hud1,
    messages: [{
      text: TEST_IFRAME_URI,
      category: CATEGORY_NETWORK,
      severity: SEVERITY_LOG,
    }],
  }).then(() => {
    let hud2 = HUDService.getHudByWindow(tab2.linkedBrowser.contentWindow);
    let outputNode2 = hud2.outputNode;

    isnot(outputNode1, outputNode2,
      "the two HUD outputNodes must be different");

    let msg = "Didn't find the iframe network request in tab2";
    testLogEntry(outputNode2, TEST_IFRAME_URI, msg, true, true);

    closeConsole(tab2).then(() => {
      gBrowser.removeTab(tab2);
      tab1 = tab2 = null;
      executeSoon(finishTest);
    });
  });
}
