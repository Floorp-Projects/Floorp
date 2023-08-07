/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    async background() {
      const URL = "https://example.com/";
      let mainWin = await browser.windows.getCurrent();
      let tab1 = await browser.tabs.create({ url: URL });
      let tab2 = await browser.tabs.create({ url: URL });

      let newWin = await browser.windows.create({ url: [URL, URL] });
      browser.test.assertEq(newWin.tabs.length, 2, "New window has 2 tabs");
      let [tab3, tab4] = newWin.tabs;

      // move tabs in both windows to index 0 in a single call
      await browser.tabs.move([tab2.id, tab4.id], { index: 0 });

      tab1 = await browser.tabs.get(tab1.id);
      browser.test.assertEq(
        tab1.windowId,
        mainWin.id,
        "tab 1 is still in main window"
      );

      tab2 = await browser.tabs.get(tab2.id);
      browser.test.assertEq(
        tab2.windowId,
        mainWin.id,
        "tab 2 is still in main window"
      );
      browser.test.assertEq(tab2.index, 0, "tab 2 moved to index 0");

      tab3 = await browser.tabs.get(tab3.id);
      browser.test.assertEq(
        tab3.windowId,
        newWin.id,
        "tab 3 is still in new window"
      );

      tab4 = await browser.tabs.get(tab4.id);
      browser.test.assertEq(
        tab4.windowId,
        newWin.id,
        "tab 4 is still in new window"
      );
      browser.test.assertEq(tab4.index, 0, "tab 4 moved to index 0");

      await browser.tabs.remove([tab1.id, tab2.id]);
      await browser.windows.remove(newWin.id);

      browser.test.notifyPass("tabs.move.multiple");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move.multiple");
  await extension.unload();
});
