/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testPrintPreview() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function () {
      await browser.tabs.printPreview();
      browser.test.assertTrue(true, "print preview entered");
      browser.test.notifyPass("tabs.printPreview");
    },
  });

  is(
    document.querySelector(".printPreviewBrowser"),
    null,
    "There shouldn't be any print preview browser"
  );

  await extension.startup();

  // Ensure we're showing the preview...
  await BrowserTestUtils.waitForCondition(() => {
    let preview = document.querySelector(".printPreviewBrowser");
    return preview && BrowserTestUtils.isVisible(preview);
  });

  gBrowser.getTabDialogBox(gBrowser.selectedBrowser).abortAllDialogs();
  // Wait for the preview to go away
  await BrowserTestUtils.waitForCondition(
    () => !document.querySelector(".printPreviewBrowser")
  );

  await extension.awaitFinish("tabs.printPreview");

  await extension.unload();
  BrowserTestUtils.removeTab(gBrowser.tabs[1]);
});
