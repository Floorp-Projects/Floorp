/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testWindowRemove() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      async function closeWindow(id) {
        let window = await browser.windows.get(id);
        return new Promise(function(resolve) {
          browser.windows.onRemoved.addListener(async function listener(windowId) {
            browser.windows.onRemoved.removeListener(listener);
            await browser.test.assertEq(windowId, window.id, "The right window was closed");
            await browser.test.assertRejects(
              browser.windows.get(windowId),
              new RegExp(`Invalid window ID: ${windowId}`),
              "The window was really closed.");
            resolve();
          });
          browser.windows.remove(id);
        });
      }

      browser.test.log("Create a new window and close it by its ID");
      let newWindow = await browser.windows.create();
      await closeWindow(newWindow.id);

      browser.test.log("Create a new window and close it by WINDOW_ID_CURRENT");
      await browser.windows.create();
      await closeWindow(browser.windows.WINDOW_ID_CURRENT);

      browser.test.log("Assert failure for bad parameter.");
      await browser.test.assertThrows(
        () => browser.windows.remove(-3),
        /-3 is too small \(must be at least -2\)/,
        "Invalid windowId throws");

      browser.test.notifyPass("window-remove");
    },
  });

  await extension.startup();
  await extension.awaitFinish("window-remove");
  await extension.unload();
});
