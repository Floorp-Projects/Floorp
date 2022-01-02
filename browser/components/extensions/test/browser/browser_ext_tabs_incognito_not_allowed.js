/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testExecuteScriptIncognitoNotAllowed() {
  const url =
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/file_iframe_document.html";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      // captureTab requires all_urls permission
      permissions: ["<all_urls>", "tabs", "tabHide"],
    },
    background() {
      browser.test.onMessage.addListener(async pbw => {
        // expect one tab from the non-pb window
        let tabs = await browser.tabs.query({ windowId: pbw.windowId });
        browser.test.assertEq(
          0,
          tabs.length,
          "unable to query tabs in private window"
        );
        tabs = await browser.tabs.query({ active: true });
        browser.test.assertEq(
          1,
          tabs.length,
          "unable to query active tab in private window"
        );
        browser.test.assertTrue(
          tabs[0].windowId != pbw.windowId,
          "unable to query active tab in private window"
        );

        // apis that take a tabId
        let tabIdAPIs = [
          "captureTab",
          "detectLanguage",
          "duplicate",
          "get",
          "hide",
          "reload",
          "getZoomSettings",
          "getZoom",
          "toggleReaderMode",
        ];
        for (let name of tabIdAPIs) {
          await browser.test.assertRejects(
            browser.tabs[name](pbw.tabId),
            /Invalid tab ID/,
            `should not be able to ${name}`
          );
        }
        await browser.test.assertRejects(
          browser.tabs.captureVisibleTab(pbw.windowId),
          /Invalid window ID/,
          "should not be able to duplicate"
        );
        await browser.test.assertRejects(
          browser.tabs.create({
            windowId: pbw.windowId,
            url: "http://mochi.test/",
          }),
          /Invalid window ID/,
          "unable to create tab in private window"
        );
        await browser.test.assertRejects(
          browser.tabs.executeScript(pbw.tabId, { code: "document.URL" }),
          /Invalid tab ID/,
          "should not be able to executeScript"
        );
        let currentTab = await browser.tabs.getCurrent();
        browser.test.assertTrue(
          !currentTab,
          "unable to get current tab in private window"
        );
        await browser.test.assertRejects(
          browser.tabs.highlight({ windowId: pbw.windowId, tabs: [pbw.tabId] }),
          /Invalid window ID/,
          "should not be able to highlight"
        );
        await browser.test.assertRejects(
          browser.tabs.insertCSS(pbw.tabId, {
            code: "* { background: rgb(42, 42, 42) }",
          }),
          /Invalid tab ID/,
          "should not be able to insertCSS"
        );
        await browser.test.assertRejects(
          browser.tabs.move(pbw.tabId, {
            index: 0,
            windowId: tabs[0].windowId,
          }),
          /Invalid tab ID/,
          "unable to move tab to private window"
        );
        await browser.test.assertRejects(
          browser.tabs.move(tabs[0].id, { index: 0, windowId: pbw.windowId }),
          /Invalid window ID/,
          "unable to move tab to private window"
        );
        await browser.test.assertRejects(
          browser.tabs.printPreview(),
          /Cannot access activeTab/,
          "unable to printpreview tab"
        );
        await browser.test.assertRejects(
          browser.tabs.removeCSS(pbw.tabId, {}),
          /Invalid tab ID/,
          "unable to remove tab css"
        );
        await browser.test.assertRejects(
          browser.tabs.sendMessage(pbw.tabId, "test"),
          /Could not establish connection/,
          "unable to sendmessage"
        );
        await browser.test.assertRejects(
          browser.tabs.setZoomSettings(pbw.tabId, {}),
          /Invalid tab ID/,
          "should not be able to set zoom settings"
        );
        await browser.test.assertRejects(
          browser.tabs.setZoom(pbw.tabId, 3),
          /Invalid tab ID/,
          "should not be able to set zoom"
        );
        await browser.test.assertRejects(
          browser.tabs.update(pbw.tabId, {}),
          /Invalid tab ID/,
          "should not be able to update tab"
        );
        await browser.test.assertRejects(
          browser.tabs.moveInSuccession([pbw.tabId], tabs[0].id),
          /Invalid tab ID/,
          "should not be able to moveInSuccession"
        );
        await browser.test.assertRejects(
          browser.tabs.moveInSuccession([tabs[0].id], pbw.tabId),
          /Invalid tab ID/,
          "should not be able to moveInSuccession"
        );

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
