/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowsEvents() {
  function background() {
    browser.windows.onCreated.addListener(function listener(window) {
      browser.windows.onCreated.removeListener(listener);
      browser.test.assertTrue(Number.isInteger(window.id),
                              "Window object's id is an integer");
      browser.test.assertEq("normal", window.type,
                            "Window object returned with the correct type");
      browser.test.sendMessage("window-created", window.id);
    });

    browser.windows.onRemoved.addListener(function listener(windowId) {
      browser.windows.onRemoved.removeListener(listener);
      browser.test.assertTrue(Number.isInteger(windowId),
                              "windowId is an integer");
      browser.test.sendMessage(`window-removed-${windowId}`);
      browser.test.notifyPass("windows.events");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})()`,
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let windowId = yield extension.awaitMessage("window-created");
  yield BrowserTestUtils.closeWindow(win1);
  yield extension.awaitMessage(`window-removed-${windowId}`);
  yield extension.awaitFinish("windows.events");
  yield extension.unload();
});
