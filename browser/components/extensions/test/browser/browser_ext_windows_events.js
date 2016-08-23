/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

add_task(function* testWindowsEvents() {
  function background() {
    browser.windows.onCreated.addListener(function listener(window) {
      browser.test.log(`onCreated: windowId=${window.id}`);

      browser.test.assertTrue(Number.isInteger(window.id),
                              "Window object's id is an integer");
      browser.test.assertEq("normal", window.type,
                            "Window object returned with the correct type");
      browser.test.sendMessage("window-created", window.id);
    });

    let lastWindowId;
    browser.windows.onFocusChanged.addListener(function listener(windowId) {
      browser.test.log(`onFocusChange: windowId=${windowId} lastWindowId=${lastWindowId}`);

      browser.test.assertTrue(lastWindowId !== windowId,
                              "onFocusChanged fired once for the given window");
      lastWindowId = windowId;

      browser.test.assertTrue(Number.isInteger(windowId),
                              "windowId is an integer");

      browser.windows.getLastFocused().then(window => {
        browser.test.assertEq(windowId, window.id,
                              "Last focused window has the correct id");
        browser.test.sendMessage(`window-focus-changed`, window.id);
      });
    });

    browser.windows.onRemoved.addListener(function listener(windowId) {
      browser.test.log(`onRemoved: windowId=${windowId}`);

      browser.test.assertTrue(Number.isInteger(windowId),
                              "windowId is an integer");
      browser.test.sendMessage(`window-removed`, windowId);
      browser.test.notifyPass("windows.events");
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})()`,
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  let {Management: {global: {WindowManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let currentWindow = window;
  let currentWindowId = WindowManager.getId(currentWindow);
  info(`Current window ID: ${currentWindowId}`);

  info(`Create browser window 1`);
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win1Id = yield extension.awaitMessage("window-created");
  info(`Window 1 ID: ${win1Id}`);

  let winId = yield extension.awaitMessage(`window-focus-changed`);
  is(winId, win1Id, "Got focus change event for the correct window ID.");

  info(`Create browser window 2`);
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2Id = yield extension.awaitMessage("window-created");
  info(`Window 2 ID: ${win2Id}`);

  winId = yield extension.awaitMessage(`window-focus-changed`);
  is(winId, win2Id, "Got focus change event for the correct window ID.");

  info(`Focus browser window 1`);
  yield focusWindow(win1);

  winId = yield extension.awaitMessage(`window-focus-changed`);
  is(winId, win1Id, "Got focus change event for the correct window ID.");

  info(`Close browser window 2`);
  yield BrowserTestUtils.closeWindow(win2);

  winId = yield extension.awaitMessage(`window-removed`);
  is(winId, win2Id, "Got removed event for the correct window ID.");

  info(`Close browser window 1`);
  yield BrowserTestUtils.closeWindow(win1);

  winId = yield extension.awaitMessage(`window-removed`);
  is(winId, win1Id, "Got removed event for the correct window ID.");

  winId = yield extension.awaitMessage(`window-focus-changed`);
  is(winId, currentWindowId, "Got focus change event for the correct window ID.");

  yield extension.awaitFinish("windows.events");
  yield extension.unload();
});
