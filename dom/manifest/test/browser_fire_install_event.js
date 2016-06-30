//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, gBrowser */
"use strict";
const { PromiseMessage } = Cu.import("resource://gre/modules/PromiseMessage.jsm", {});
const testPath = "/browser/dom/manifest/test/file_reg_install_event.html";
const defaultURL = new URL("http://example.org/browser/dom/manifest/test/file_testserver.sjs");
const testURL = new URL(defaultURL);
testURL.searchParams.append("file", testPath);

// Enable window.oninstall, so we can fire events at it.
function enableOnInstallPref() {
  const ops = {
    "set": [
      ["dom.manifest.oninstall", true],
    ],
  };
  return SpecialPowers.pushPrefEnv(ops);
}

// Send a message for the even to be fired.
// This cause file_reg_install_event.html to be dynamically change.
function* theTest(aBrowser) {
  aBrowser.allowEvents = true;
  // Resolves when we get a custom event with the correct name
  const responsePromise = new Promise((resolve) => {
    aBrowser.contentDocument.addEventListener("dom.manifest.oninstall", resolve);
  });
  const mm = aBrowser.messageManager;
  const msgKey = "DOM:Manifest:FireInstallEvent";
  const { data: { success } } = yield PromiseMessage.send(mm, msgKey);
  ok(success, "message sent and received successfully.");
  const { detail: { result } } = yield responsePromise;
  ok(result, "the page sent us an acknowledgment as a custom event");
}

// Open a tab and run the test
add_task(function*() {
  yield enableOnInstallPref();
  let tabOptions = {
    gBrowser: gBrowser,
    url: testURL.href,
  };
  yield BrowserTestUtils.withNewTab(
    tabOptions,
    theTest
  );
});
