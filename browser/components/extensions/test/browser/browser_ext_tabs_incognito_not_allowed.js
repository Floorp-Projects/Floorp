/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testExecuteScriptIncognitoNotAllowed() {
  const url = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/file_iframe_document.html";

  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "not_allowed",
    manifest: {
      "permissions": ["http://mochi.test/", "tabs"],
    },
    background() {
      browser.test.onMessage.addListener(async pbw => {
        // Test that executeScript and insertCSS will fail to
        // load into an incognito window.
        await browser.test.assertRejects(browser.tabs.executeScript(pbw.tabId, {code: "document.URL"}),
                                         /Invalid tab ID/,
                                         "should not be able to executeScript");
        await browser.test.assertRejects(browser.tabs.insertCSS(pbw.tabId, {code: "* { background: rgb(42, 42, 42) }"}),
                                         /Invalid tab ID/,
                                         "should not be able to insertCSS");
        browser.test.notifyPass("pass");
      });
    },
  });

  let winData = await getIncognitoWindow(url);
  await extension.startup();

  extension.sendMessage(winData.details);

  await extension.awaitFinish("pass");
  await BrowserTestUtils.closeWindow(winData.win);
  await extension.unload();
});
