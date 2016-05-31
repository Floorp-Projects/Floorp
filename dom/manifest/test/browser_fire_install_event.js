//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, PromiseMessage, gBrowser */
"use strict";
const { PromiseMessage } = Cu.import("resource://gre/modules/PromiseMessage.jsm", {});
const testPath = "/browser/dom/manifest/test/file_reg_install_event.html";
const defaultURL = new URL("http://example.org/browser/dom/manifest/test/file_testserver.sjs");
const testURL = new URL(defaultURL);
testURL.searchParams.append("file", testPath);

// Send a message for the even to be fired.
// This cause file_reg_install_event.html to be dynamically change.
function* theTest(aBrowser) {
  const mm = aBrowser.messageManager;
  const msgKey = "DOM:Manifest:FireInstallEvent";
  const initialText = aBrowser.contentWindowAsCPOW.document.body.innerHTML.trim()
  is(initialText, '<h1 id="output">waiting for event</h1>', "should be waiting for event");
  const { data: { success } } = yield PromiseMessage.send(mm, msgKey);
  ok(success, "message sent and received successfully.");
  const eventText = aBrowser.contentWindowAsCPOW.document.body.innerHTML.trim();
  is(eventText, '<h1 id="output">event received!</h1>', "text of the page should have changed.");
};

// Open a tab and run the test
add_task(function*() {
  let tabOptions = {
    gBrowser: gBrowser,
    url: testURL.href,
  };
  yield BrowserTestUtils.withNewTab(
    tabOptions,
    theTest
  );
});
