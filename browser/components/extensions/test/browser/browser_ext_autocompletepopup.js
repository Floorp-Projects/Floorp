/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testAutocompletePopup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_popup": "page.html",
        "browser_style": false,
      },
      "page_action": {
        "default_popup": "page.html",
        "browser_style": false,
      },
    },
    background: async function() {
      let [tab] = await browser.tabs.query({active: true, currentWindow: true});
      await browser.pageAction.show(tab.id);
      browser.test.sendMessage("ready");
    },
    files: {
      "page.html": `<!DOCTYPE html>
        <html>
          <head><meta charset="utf-8"></head>
          <body>
          <div>
          <input placeholder="Test input" id="test-input" list="test-list" />
          <datalist id="test-list">
            <option value="aa">
            <option value="ab">
            <option value="ae">
            <option value="af">
            <option value="ak">
            <option value="am">
            <option value="an">
            <option value="ar">
          </datalist>
          </div>
          </body>
        </html>`,
    },
  });

  async function testDatalist(browser, doc) {
    let autocompletePopup = doc.getElementById("PopupAutoComplete");
    let opened = promisePopupShown(autocompletePopup);
    info("click in test-input now");
    // two clicks to open
    await BrowserTestUtils.synthesizeMouseAtCenter("#test-input", {}, browser);
    await BrowserTestUtils.synthesizeMouseAtCenter("#test-input", {}, browser);
    info("wait for opened event");
    await opened;
    // third to close
    let closed = promisePopupHidden(autocompletePopup);
    info("click in test-input now");
    await BrowserTestUtils.synthesizeMouseAtCenter("#test-input", {}, browser);
    info("wait for closed event");
    await closed;
    // If this didn't work, we hang. Other tests deal with testing the actual functionality of datalist.
    ok(true, "datalist popup has been shown");
  }
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  await extension.startup();
  await extension.awaitMessage("ready");

  clickPageAction(extension);
  // intentional misspell so eslint is ok with browser in background script.
  let bowser = await awaitExtensionPanel(extension);
  ok(!!bowser, "panel opened with browser");
  await testDatalist(bowser, document);
  closePageAction(extension);
  await new Promise(resolve => setTimeout(resolve, 0));

  clickBrowserAction(extension);
  bowser = await awaitExtensionPanel(extension);
  ok(!!bowser, "panel opened with browser");
  await testDatalist(bowser, document);
  closeBrowserAction(extension);
  await new Promise(resolve => setTimeout(resolve, 0));

  await extension.unload();
  await BrowserTestUtils.removeTab(tab);
});
