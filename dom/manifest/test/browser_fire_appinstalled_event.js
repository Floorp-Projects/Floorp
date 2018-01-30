//Used by JSHint:
/*global Cu, BrowserTestUtils, ok, add_task, gBrowser */
"use strict";
const { PromiseMessage } = Cu.import("resource://gre/modules/PromiseMessage.jsm", {});
const testPath = "/browser/dom/manifest/test/file_reg_appinstalled_event.html";
const defaultURL = new URL("http://example.org/browser/dom/manifest/test/file_testserver.sjs");
const testURL = new URL(defaultURL);
testURL.searchParams.append("file", testPath);

// Enable window.onappinstalled, so we can fire events at it.
function enableOnAppInstalledPref() {
  const ops = {
    "set": [
      ["dom.manifest.onappinstalled", true],
    ],
  };
  return SpecialPowers.pushPrefEnv(ops);
}

// Send a message for the even to be fired.
// This cause file_reg_install_event.html to be dynamically change.
async function theTest(aBrowser) {
  aBrowser.allowEvents = true;
  let waitForInstall = ContentTask.spawn(aBrowser, null, async function() {
    await ContentTaskUtils.waitForEvent(content.window, "appinstalled");
  });
  const { data: { success } } = await PromiseMessage
    .send(aBrowser.messageManager, "DOM:Manifest:FireAppInstalledEvent");
  ok(success, "message sent and received successfully.");
  try {
    await waitForInstall;
    ok(true, "AppInstalled event fired");
  } catch (err) {
    ok(false, "AppInstalled event didn't fire: " + err.message);
  }
}

// Open a tab and run the test
add_task(async function() {
  await enableOnAppInstalledPref();
  let tabOptions = {
    gBrowser: gBrowser,
    url: testURL.href,
  };
  await BrowserTestUtils.withNewTab(
    tabOptions,
    theTest
  );
});
