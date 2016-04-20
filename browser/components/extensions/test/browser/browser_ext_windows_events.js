/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowsEvents() {
  function background() {
    browser.windows.onCreated.addListener(function listener(window) {
      browser.test.assertTrue(Number.isInteger(window.id),
                              "Window object's id is an integer");
      browser.test.assertEq("normal", window.type,
                            "Window object returned with the correct type");
      browser.test.sendMessage("window-created", window.id);
    });

    let lastWindowId;
    browser.windows.onFocusChanged.addListener(function listener(windowId) {
      browser.test.assertTrue(lastWindowId !== windowId,
                              "onFocusChanged fired once for the given window");
      lastWindowId = windowId;
      browser.test.assertTrue(Number.isInteger(windowId),
                              "windowId is an integer");
      browser.windows.getLastFocused().then(window => {
        browser.test.assertEq(windowId, window.id,
                              "Last focused window has the correct id");
        browser.test.sendMessage(`window-focus-changed-${windowId}`);
      });
    });

    browser.windows.onRemoved.addListener(function listener(windowId) {
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

  let {WindowManager} = Cu.import("resource://gre/modules/Extension.jsm", {});
  let currentWindow = window;
  let currentWindowId = WindowManager.getId(currentWindow);

  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win1Id = yield extension.awaitMessage("window-created");
  yield extension.awaitMessage(`window-focus-changed-${win1Id}`);

  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2Id = yield extension.awaitMessage("window-created");
  yield extension.awaitMessage(`window-focus-changed-${win2Id}`);

  yield focusWindow(win1);
  yield extension.awaitMessage(`window-focus-changed-${win1Id}`);

  yield BrowserTestUtils.closeWindow(win2);
  yield extension.awaitMessage(`window-removed-${win2Id}`);

  yield BrowserTestUtils.closeWindow(win1);
  yield extension.awaitMessage(`window-removed-${win1Id}`);

  yield extension.awaitMessage(`window-focus-changed-${currentWindowId}`);
  yield extension.awaitFinish("windows.events");
  yield extension.unload();
});
