/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testPrintPreview() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background: async function() {
      await browser.tabs.printPreview();
      browser.test.assertTrue(true, "print preview entered");
      browser.test.notifyPass("tabs.printPreview");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.printPreview");
  await extension.unload();

  let ppTab = PrintUtils.shouldSimplify ?
      PrintPreviewListener._simplifiedPrintPreviewTab :
      PrintPreviewListener._printPreviewTab;

  let ppToolbar = document.getElementById("print-preview-toolbar");

  is(window.gInPrintPreviewMode, true, "window in print preview mode");

  isnot(ppTab, null, "print preview tab created");
  isnot(ppTab.linkedBrowser, null, "print preview browser created");
  isnot(ppToolbar, null, "print preview toolbar created");

  is(ppTab, gBrowser.selectedTab, "print preview tab selected");
  is(ppTab.linkedBrowser.currentURI.spec, "about:printpreview", "print preview browser url correct");

  PrintUtils.exitPrintPreview();
  await BrowserTestUtils.waitForCondition(() => !window.gInPrintPreviewMode);

  BrowserTestUtils.removeTab(gBrowser.tabs[1]);
});
