/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL =
  "http://example.com/browser/browser/base/content/test/tabcrashed/file_contains_emptyiframe.html";
const DOMAIN = "example.com";

/**
 * This is really a crashtest, but because we need PrintUtils this is written as a browser test.
 * Test that when we don't crash when trying to print a document in the following scenario -
 * A top level document has an iframe of different origin embedded (here example.com has test1.example.com iframe embedded)
 * and they both set their document.domain to be "example.com".
 */
add_task(async function test() {
  // 1. Open a new tab and wait for it to load the top level doc
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  let browser = newTab.linkedBrowser;

  // 2. Navigate the iframe within the doc and wait for the load to complete
  await SpecialPowers.spawn(browser, [], async function() {
    const iframe = content.document.querySelector("iframe");
    const loaded = new Promise(resolve => {
      iframe.addEventListener(
        "load",
        () => {
          resolve();
        },
        { once: true }
      );
    });
    iframe.src =
      "http://test1.example.com/browser/browser/base/content/test/tabcrashed/file_iframe.html";
    await loaded;
  });

  // 3. Change the top level document's domain
  await SpecialPowers.spawn(browser, [DOMAIN], async function(domain) {
    content.document.domain = domain;
  });

  // 4. Get the reference to the iframe and change its domain
  const iframe = await SpecialPowers.spawn(browser, [], () => {
    return content.document.querySelector("iframe").browsingContext;
  });

  await SpecialPowers.spawn(iframe, [DOMAIN], domain => {
    content.document.domain = domain;
  });

  // 5. Try to print things
  ok(
    !gInPrintPreviewMode,
    "Should NOT be in print preview mode at the start of this test."
  );

  // Enter print preview
  let ppBrowser = PrintPreviewListener.getPrintPreviewBrowser();
  let printPreviewEntered = BrowserTestUtils.waitForMessage(
    ppBrowser.messageManager,
    "Printing:Preview:Entered"
  );
  document.getElementById("cmd_printPreview").doCommand();
  await printPreviewEntered;

  // Ensure we are in print preview
  await BrowserTestUtils.waitForCondition(
    () => gInPrintPreviewMode,
    "Should be in print preview mode now."
  );
  ok(true, "We did not crash.");

  // We haven't crashed! Exit the print preview.
  await BrowserTestUtils.switchTab(gBrowser, () => {
    PrintUtils.exitPrintPreview();
  });

  await BrowserTestUtils.waitForCondition(() => !window.gInPrintPreviewMode);
  info("We are not in print preview anymore.");

  BrowserTestUtils.removeTab(newTab);
});
