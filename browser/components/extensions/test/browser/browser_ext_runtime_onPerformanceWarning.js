/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {
  Management: {
    global: { tabTracker },
  },
} = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

const {
  ExtensionUtils: { promiseObserved },
} = ChromeUtils.importESModule("resource://gre/modules/ExtensionUtils.sys.mjs");

class TestHangReport {
  constructor(addonId, scriptBrowser) {
    this.addonId = addonId;
    this.scriptBrowser = scriptBrowser;
    this.QueryInterface = ChromeUtils.generateQI(["nsIHangReport"]);
  }

  userCanceled() {}
  terminateScript() {}

  isReportForBrowserOrChildren(frameLoader) {
    return (
      !this.scriptBrowser || this.scriptBrowser.frameLoader === frameLoader
    );
  }
}

function dispatchHangReport(extensionId, scriptBrowser) {
  const hangObserved = promiseObserved("process-hang-report");

  Services.obs.notifyObservers(
    new TestHangReport(extensionId, scriptBrowser),
    "process-hang-report"
  );

  return hangObserved;
}

function background() {
  let onPerformanceWarningDetails = null;

  browser.runtime.onPerformanceWarning.addListener(details => {
    onPerformanceWarningDetails = details;
  });

  browser.test.onMessage.addListener(message => {
    if (message === "get-on-performance-warning-details") {
      browser.test.sendMessage(
        "on-performance-warning-details",
        onPerformanceWarningDetails
      );
      onPerformanceWarningDetails = null;
    }
  });
}

async function expectOnPerformanceWarningDetails(
  extension,
  expectedOnPerformanceWarningDetails
) {
  extension.sendMessage("get-on-performance-warning-details");

  let actualOnPerformanceWarningDetails = await extension.awaitMessage(
    "on-performance-warning-details"
  );
  Assert.deepEqual(
    actualOnPerformanceWarningDetails,
    expectedOnPerformanceWarningDetails,
    expectedOnPerformanceWarningDetails
      ? "runtime.onPerformanceWarning fired with correct details"
      : "runtime.onPerformanceWarning didn't fire"
  );
}

add_task(async function test_should_fire_on_process_hang_report() {
  const description =
    "Slow extension content script caused a page hang, user was warned.";

  const extension = ExtensionTestUtils.loadExtension({ background });
  await extension.startup();

  const notificationPromise = BrowserTestUtils.waitForGlobalNotificationBar(
    window,
    "process-hang"
  );

  const tabs = await Promise.all([
    BrowserTestUtils.openNewForegroundTab(gBrowser),
    BrowserTestUtils.openNewForegroundTab(gBrowser),
  ]);

  // Warning event shouldn't have fired initially.
  await expectOnPerformanceWarningDetails(extension, null);

  // Hang report fired for the extension and first tab. Warning event with first
  // tab ID expected.
  await dispatchHangReport(extension.id, tabs[0].linkedBrowser);
  await expectOnPerformanceWarningDetails(extension, {
    category: "content_script",
    severity: "high",
    description,
    tabId: tabTracker.getId(tabs[0]),
  });

  // Hang report fired for different extension, no warning event expected.
  await dispatchHangReport("wrong-addon-id", tabs[0].linkedBrowser);
  await expectOnPerformanceWarningDetails(extension, null);

  // Non-extension hang report fired, no warning event expected.
  await dispatchHangReport(null, tabs[0].linkedBrowser);
  await expectOnPerformanceWarningDetails(extension, null);

  // Hang report fired for the extension and second tab. Warning event with
  // second tab ID expected.
  await dispatchHangReport(extension.id, tabs[1].linkedBrowser);
  await expectOnPerformanceWarningDetails(extension, {
    category: "content_script",
    severity: "high",
    description,
    tabId: tabTracker.getId(tabs[1]),
  });

  // Hang report fired for the extension with no associated tab. Warning event
  // with no tab ID expected.
  await dispatchHangReport(extension.id, null);
  await expectOnPerformanceWarningDetails(extension, {
    category: "content_script",
    severity: "high",
    description,
  });

  await Promise.all(tabs.map(BrowserTestUtils.removeTab));
  await extension.unload();

  // Wait for the process-hang warning bar to be displayed, then ensure it's
  // cleared to avoid clobbering other tests.
  const notification = await notificationPromise;
  Assert.ok(notification.isConnected, "Notification still present");
  notification.buttonContainer.querySelector("[label='Stop']").click();
});
