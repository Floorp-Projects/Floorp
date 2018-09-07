// /* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
// /* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function test_buttons() {
  const manifest = {
    permissions: ["menus"],
  };

  function background() {
    function onclick(info) {
      browser.test.sendMessage("click", info);
    }
    browser.menus.create({title: "modify", onclick}, () => {
      browser.test.sendMessage("ready");
    });
  }

  const extension = ExtensionTestUtils.loadExtension({manifest, background});
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);

  await extension.startup();
  await extension.awaitMessage("ready");

  for (let i of [0, 1, 2]) {
    const menu = await openContextMenu();
    const items = menu.getElementsByAttribute("label", "modify");
    await closeExtensionContextMenu(items[0], {button: i});
    const info = await extension.awaitMessage("click");
    is(info.button, i, `Button value should be ${i}`);
  }

  BrowserTestUtils.removeTab(tab);
  await extension.unload();
});
