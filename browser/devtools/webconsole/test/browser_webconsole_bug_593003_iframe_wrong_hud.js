/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-593003-iframe-wrong-hud.html";

const TEST_IFRAME_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-593003-iframe-wrong-hud-iframe.html";

const TEST_DUMMY_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

let tab1, tab2;

function test() {
  addTab(TEST_URI);
  tab1 = tab;
  browser.addEventListener("load", tab1Loaded, true);
}

function tab1Loaded(aEvent) {
  browser.removeEventListener(aEvent.type, tab1Loaded, true);
  content.console.log("FOO");
  openConsole(null, function() {
    tab2 = gBrowser.addTab(TEST_DUMMY_URI);
    gBrowser.selectedTab = tab2;
    gBrowser.selectedBrowser.addEventListener("load", tab2Loaded, true);
  });
}

function tab2Loaded(aEvent) {
  tab2.linkedBrowser.removeEventListener(aEvent.type, tab2Loaded, true);

  openConsole(gBrowser.selectedTab, function() {
    tab1.linkedBrowser.addEventListener("load", tab1Reloaded, true);
    tab1.linkedBrowser.contentWindow.location.reload();
  });
}

function tab1Reloaded(aEvent) {
  tab1.linkedBrowser.removeEventListener(aEvent.type, tab1Reloaded, true);

  let hud1 = HUDService.getHudByWindow(tab1.linkedBrowser.contentWindow);
  let outputNode1 = hud1.outputNode;

  waitForSuccess({
    name: "iframe network request displayed in tab1",
    validatorFn: function()
    {
      let selector = ".webconsole-msg-url[value='" + TEST_IFRAME_URI +"']";
      return outputNode1.querySelector(selector);
    },
    successFn: function()
    {
      let hud2 = HUDService.getHudByWindow(tab2.linkedBrowser.contentWindow);
      let outputNode2 = hud2.outputNode;

      isnot(outputNode1, outputNode2,
            "the two HUD outputNodes must be different");

      let msg = "Didn't find the iframe network request in tab2";
      testLogEntry(outputNode2, TEST_IFRAME_URI, msg, true, true);

      testEnd();
    },
    failureFn: testEnd,
  });
}

function testEnd() {
  closeConsole(tab2, function() {
    gBrowser.removeTab(tab2);
    tab1 = tab2 = null;
    executeSoon(finishTest);
  });
}
