/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testExecuteScript_at_about_blank() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      host_permissions: ["*://*/*"], // allows script in top-level about:blank.
    },
    async background() {
      try {
        const tab = await browser.tabs.create({ url: "about:blank" });
        const result = await browser.tabs.executeScript(tab.id, {
          code: "location.href",
          matchAboutBlank: true,
        });
        browser.test.assertEq(
          "about:blank",
          result[0],
          "Script executed correctly in new tab"
        );
        await browser.tabs.remove(tab.id);
        browser.test.notifyPass("executeScript");
      } catch (e) {
        browser.test.fail(`Error: ${e} :: ${e.stack}`);
        browser.test.notifyFail("executeScript");
      }
    },
  });
  await extension.startup();
  await extension.awaitFinish("executeScript");
  await extension.unload();
});
