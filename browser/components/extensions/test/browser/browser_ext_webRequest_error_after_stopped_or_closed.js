/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const SLOW_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://www.example.com"
  ) + "file_slowed_document.sjs";

async function runTest(stopLoadFunc) {
  async function background() {
    let urls = ["https://www.example.com/*"];
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.sendMessage("done", {
          msg: "onCompleted",
          requestId: details.requestId,
        });
      },
      { urls }
    );

    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.sendMessage("onBeforeRequest", {
          requestId: details.requestId,
        });
      },
      { urls },
      ["blocking"]
    );

    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.sendMessage("done", {
          msg: "onErrorOccurred",
          requestId: details.requestId,
        });
      },
      { urls }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "https://www.example.com/*",
      ],
    },
    background,
  });
  await extension.startup();

  // Open a SLOW_PAGE and don't wait for it to load
  let slowTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SLOW_PAGE,
    false
  );

  stopLoadFunc(slowTab);

  // Retrieve the requestId from onBeforeRequest
  let requestIdOnBeforeRequest = await extension.awaitMessage(
    "onBeforeRequest"
  );

  // Now verify that we got the correct event and request id
  let doneMessage = await extension.awaitMessage("done");

  // We shouldn't get the onCompleted message here
  is(doneMessage.msg, "onErrorOccurred", "received onErrorOccurred message");
  is(
    requestIdOnBeforeRequest.requestId,
    doneMessage.requestId,
    "request Ids match"
  );

  BrowserTestUtils.removeTab(slowTab);
  await extension.unload();
}

/**
 * Check that after we cancel a slow page load, we get an error associated with
 * our request.
 */
add_task(async function test_click_stop_button() {
  await runTest(async () => {
    // Stop the load
    let stopButton = document.getElementById("stop-button");
    await TestUtils.waitForCondition(() => {
      return !stopButton.disabled;
    });
    stopButton.click();
  });
});

/**
 * Check that after we close the tab corresponding to a slow page load,
 * that we get an error associated with our request.
 */
add_task(async function test_remove_tab() {
  await runTest(slowTab => {
    // Remove the tab
    BrowserTestUtils.removeTab(slowTab);
  });
});
