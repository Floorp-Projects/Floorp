/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    incognitoOverride: "spanning",
    async background() {
      const URL = "https://example.com/";
      let mainWindow = await browser.windows.getCurrent();
      let newWindow = await browser.windows.create({
        url: [URL, URL],
      });
      let privateWindow = await browser.windows.create({
        incognito: true,
        url: [URL, URL],
      });

      browser.tabs.onUpdated.addListener(() => {
        // Bug 1398272: Adding onUpdated listener broke tab IDs across windows.
      });

      let tab = newWindow.tabs[0].id;
      let privateTab = privateWindow.tabs[0].id;

      // Assuming that this windowId does not exist.
      await browser.test.assertRejects(
        browser.tabs.move(tab, { windowId: 123144576, index: 0 }),
        /Invalid window/,
        "Should receive invalid window error"
      );

      // Test that a tab cannot be moved to a private window.
      let moved = await browser.tabs.move(tab, {
        windowId: privateWindow.id,
        index: 0,
      });
      browser.test.assertEq(
        moved.length,
        0,
        "tab was not moved to private window"
      );
      // Test that a private tab cannot be moved to a non-private window.
      moved = await browser.tabs.move(privateTab, {
        windowId: newWindow.id,
        index: 0,
      });
      browser.test.assertEq(
        moved.length,
        0,
        "tab was not moved from private window"
      );

      // Verify tabs did not move between windows via another query.
      let windows = await browser.windows.getAll({ populate: true });
      let newWin2 = windows.find(w => w.id === newWindow.id);
      browser.test.assertTrue(newWin2, "Found window");
      browser.test.assertEq(
        newWin2.tabs.length,
        2,
        "Window still has two tabs"
      );
      for (let origTab of newWindow.tabs) {
        browser.test.assertTrue(
          newWin2.tabs.find(t => t.id === origTab.id),
          `Window still has tab ${origTab.id}`
        );
      }

      let privateWin2 = windows.find(w => w.id === privateWindow.id);
      browser.test.assertTrue(privateWin2 !== null, "Found private window");
      browser.test.assertEq(
        privateWin2.incognito,
        true,
        "Private window is still private"
      );
      browser.test.assertEq(
        privateWin2.tabs.length,
        2,
        "Private window still has two tabs"
      );
      for (let origTab of privateWindow.tabs) {
        browser.test.assertTrue(
          privateWin2.tabs.find(t => t.id === origTab.id),
          `Private window still has tab ${origTab.id}`
        );
      }

      // Move a tab from one non-private window to another
      await browser.tabs.move(tab, { windowId: mainWindow.id, index: 0 });

      mainWindow = await browser.windows.get(mainWindow.id, { populate: true });
      browser.test.assertTrue(
        mainWindow.tabs.find(t => t.id === tab),
        "Moved tab is in main window"
      );

      newWindow = await browser.windows.get(newWindow.id, { populate: true });
      browser.test.assertEq(
        newWindow.tabs.length,
        1,
        "New window has 1 tab left"
      );
      browser.test.assertTrue(
        newWindow.tabs[0].id != tab,
        "Moved tab is no longer in original window"
      );

      await browser.windows.remove(newWindow.id);
      await browser.windows.remove(privateWindow.id);
      await browser.tabs.remove(tab);

      browser.test.notifyPass("tabs.move.window");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move.window");
  await extension.unload();
});

add_task(async function test_currentWindowAfterTabMoved() {
  const files = {
    "current.html": "<meta charset=utf-8><script src=current.js></script>",
    "current.js": function () {
      browser.test.onMessage.addListener(msg => {
        if (msg === "current") {
          browser.windows.getCurrent(win => {
            browser.test.sendMessage("id", win.id);
          });
        }
      });
      browser.test.sendMessage("ready");
    },
  };

  async function background() {
    let tabId;

    const url = browser.runtime.getURL("current.html");

    browser.test.onMessage.addListener(async msg => {
      if (msg === "move") {
        await browser.windows.create({ tabId });
        browser.test.sendMessage("moved");
      } else if (msg === "close") {
        await browser.tabs.remove(tabId);
        browser.test.sendMessage("done");
      }
    });

    let tab = await browser.tabs.create({ url });
    tabId = tab.id;
  }

  const extension = ExtensionTestUtils.loadExtension({ files, background });

  await extension.startup();
  await extension.awaitMessage("ready");

  extension.sendMessage("current");
  const first = await extension.awaitMessage("id");

  extension.sendMessage("move");
  await extension.awaitMessage("moved");

  extension.sendMessage("current");
  const second = await extension.awaitMessage("id");

  isnot(first, second, "current window id is different after moving the tab");

  extension.sendMessage("close");
  await extension.awaitMessage("done");
  await extension.unload();
});
