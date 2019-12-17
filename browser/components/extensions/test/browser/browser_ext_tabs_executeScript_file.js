/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const FILE_URL = Services.io.newFileURI(
  new FileUtils.File(getTestFilePath("file_dummy.html"))
).spec;

add_task(async function testExecuteScript_at_file_url() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "file:///*"],
    },
    background() {
      browser.test.onMessage.addListener(async () => {
        try {
          const [tab] = await browser.tabs.query({ url: "file://*/*/*dummy*" });
          const result = await browser.tabs.executeScript(tab.id, {
            code: "location.protocol",
          });
          browser.test.assertEq(
            "file:",
            result[0],
            "Script executed correctly in new tab"
          );
          browser.test.notifyPass("executeScript");
        } catch (e) {
          browser.test.fail(`Error: ${e} :: ${e.stack}`);
          browser.test.notifyFail("executeScript");
        }
      });
    },
  });
  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE_URL);

  extension.sendMessage();
  await extension.awaitFinish("executeScript");

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
});

add_task(async function testExecuteScript_at_file_url_with_activeTab() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
      browser_action: {},
    },
    background() {
      browser.browserAction.onClicked.addListener(async tab => {
        try {
          const result = await browser.tabs.executeScript(tab.id, {
            code: "location.protocol",
          });
          browser.test.assertEq(
            "file:",
            result[0],
            "Script executed correctly in active tab"
          );
          browser.test.notifyPass("executeScript");
        } catch (e) {
          browser.test.fail(`Error: ${e} :: ${e.stack}`);
          browser.test.notifyFail("executeScript");
        }
      });

      browser.test.onMessage.addListener(async () => {
        await browser.test.assertRejects(
          browser.tabs.executeScript({ code: "location.protocol" }),
          /Missing host permission for the tab/,
          "activeTab not active yet, executeScript should be rejected"
        );
        browser.test.sendMessage("next-step");
      });
    },
  });
  await extension.startup();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, FILE_URL);

  extension.sendMessage();
  await extension.awaitMessage("next-step");

  await clickBrowserAction(extension);
  await extension.awaitFinish("executeScript");

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
});
