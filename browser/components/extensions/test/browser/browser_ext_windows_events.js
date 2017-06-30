/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

add_task(async function testWindowsEvents() {
  function background() {
    browser.windows.onCreated.addListener(window => {
      browser.test.log(`onCreated: windowId=${window.id}`);

      browser.test.assertTrue(Number.isInteger(window.id),
                              "Window object's id is an integer");
      browser.test.assertEq("normal", window.type,
                            "Window object returned with the correct type");
      browser.test.sendMessage("window-created", window.id);
    });

    let lastWindowId;
    browser.windows.onFocusChanged.addListener(async eventWindowId => {
      browser.test.log(`onFocusChange: windowId=${eventWindowId} lastWindowId=${lastWindowId}`);

      browser.test.assertTrue(lastWindowId !== eventWindowId,
                              "onFocusChanged fired once for the given window");
      lastWindowId = eventWindowId;

      browser.test.assertTrue(Number.isInteger(eventWindowId),
                              "windowId is an integer");

      let window = await browser.windows.getLastFocused();
      browser.test.sendMessage("window-focus-changed", {winId: eventWindowId, lastFocusedWindowId: window.id});
    });

    browser.windows.onRemoved.addListener(windowId => {
      browser.test.log(`onRemoved: windowId=${windowId}`);

      browser.test.assertTrue(Number.isInteger(windowId),
                              "windowId is an integer");
      browser.test.sendMessage("window-removed", windowId);
      browser.test.notifyPass("windows.events");
    });

    browser.test.sendMessage("ready", browser.windows.WINDOW_ID_NONE);
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
  });

  await extension.startup();
  const WINDOW_ID_NONE = await extension.awaitMessage("ready");

  async function awaitFocusChanged() {
    let windowInfo = await extension.awaitMessage("window-focus-changed");
    if (windowInfo.winId === WINDOW_ID_NONE) {
      info("Ignoring a superfluous WINDOW_ID_NONE (blur) event.");
      windowInfo = await extension.awaitMessage("window-focus-changed");
    }
    is(windowInfo.winId, windowInfo.lastFocusedWindowId,
       "Last focused window has the correct id");
    return windowInfo.winId;
  }

  let {Management: {global: {windowTracker}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let currentWindow = window;
  let currentWindowId = windowTracker.getId(currentWindow);
  info(`Current window ID: ${currentWindowId}`);

  info("Create browser window 1");
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win1Id = await extension.awaitMessage("window-created");
  info(`Window 1 ID: ${win1Id}`);

  // This shouldn't be necessary, but tests intermittently fail, so let's give
  // it a try.
  win1.focus();

  let winId = await awaitFocusChanged();
  is(winId, win1Id, "Got focus change event for the correct window ID.");

  info("Create browser window 2");
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  let win2Id = await extension.awaitMessage("window-created");
  info(`Window 2 ID: ${win2Id}`);

  win2.focus();

  winId = await awaitFocusChanged();
  is(winId, win2Id, "Got focus change event for the correct window ID.");

  info("Focus browser window 1");
  await focusWindow(win1);

  winId = await awaitFocusChanged();
  is(winId, win1Id, "Got focus change event for the correct window ID.");

  info("Close browser window 2");
  await BrowserTestUtils.closeWindow(win2);

  winId = await extension.awaitMessage("window-removed");
  is(winId, win2Id, "Got removed event for the correct window ID.");

  info("Close browser window 1");
  await BrowserTestUtils.closeWindow(win1);

  currentWindow.focus();

  winId = await extension.awaitMessage("window-removed");
  is(winId, win1Id, "Got removed event for the correct window ID.");

  winId = await awaitFocusChanged();
  is(winId, currentWindowId, "Got focus change event for the correct window ID.");

  await extension.awaitFinish("windows.events");
  await extension.unload();
});
