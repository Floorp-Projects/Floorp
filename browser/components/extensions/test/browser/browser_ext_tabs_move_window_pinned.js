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
      let tab = await browser.tabs.create({ url: URL });

      let newWin = await browser.windows.create({ url: URL });
      let tab2 = newWin.tabs[0];
      await browser.tabs.update(tab2.id, { pinned: true });

      // Try to move a tab before the pinned tab.  The move should be ignored.
      let moved = await browser.tabs.move(tab.id, {
        windowId: newWin.id,
        index: 0,
      });
      browser.test.assertEq(moved.length, 0, "move() returned no moved tab");

      tab = await browser.tabs.get(tab.id);
      browser.test.assertEq(
        tab.windowId,
        mainWin.id,
        "Tab stayed in its original window"
      );

      await browser.tabs.remove(tab.id);
      await browser.windows.remove(newWin.id);
      browser.test.notifyPass("tabs.move.pin");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move.pin");
  await extension.unload();
});
