/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

SimpleTest.requestCompleteLog();

add_task(async function test_windows_events_not_allowed() {
  let monitor = await startIncognitoMonitorExtension();

  function background() {
    browser.windows.onCreated.addListener(window => {
      browser.test.log(`onCreated: windowId=${window.id}`);

      browser.test.assertTrue(
        Number.isInteger(window.id),
        "Window object's id is an integer"
      );
      browser.test.assertEq(
        "normal",
        window.type,
        "Window object returned with the correct type"
      );
      browser.test.sendMessage("window-created", window.id);
    });

    let lastWindowId;
    browser.windows.onFocusChanged.addListener(async eventWindowId => {
      browser.test.log(
        `onFocusChange: windowId=${eventWindowId} lastWindowId=${lastWindowId}`
      );

      browser.test.assertTrue(
        lastWindowId !== eventWindowId,
        "onFocusChanged fired once for the given window"
      );
      lastWindowId = eventWindowId;

      browser.test.assertTrue(
        Number.isInteger(eventWindowId),
        "windowId is an integer"
      );
      let window = await browser.windows.getLastFocused();
      browser.test.sendMessage("window-focus-changed", {
        winId: eventWindowId,
        lastFocusedWindowId: window.id,
      });
    });

    browser.windows.onRemoved.addListener(windowId => {
      browser.test.log(`onRemoved: windowId=${windowId}`);

      browser.test.assertTrue(
        Number.isInteger(windowId),
        "windowId is an integer"
      );
      browser.test.sendMessage("window-removed", windowId);
      browser.test.notifyPass("windows.events");
    });

    browser.test.sendMessage("ready", browser.windows.WINDOW_ID_NONE);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {},
    background,
    incognitoOverride: "spanning",
  });

  await extension.startup();
  const WINDOW_ID_NONE = await extension.awaitMessage("ready");

  async function awaitFocusChanged() {
    let windowInfo = await extension.awaitMessage("window-focus-changed");
    if (windowInfo.winId === WINDOW_ID_NONE) {
      info("Ignoring a superfluous WINDOW_ID_NONE (blur) event.");
      windowInfo = await extension.awaitMessage("window-focus-changed");
    }
    is(
      windowInfo.winId,
      windowInfo.lastFocusedWindowId,
      "Last focused window has the correct id"
    );
    return windowInfo.winId;
  }

  const {
    Management: {
      global: { windowTracker },
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Extension.sys.mjs");

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
  let win2 = await BrowserTestUtils.openNewBrowserWindow({ private: true });
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
  is(
    winId,
    currentWindowId,
    "Got focus change event for the correct window ID."
  );

  await extension.awaitFinish("windows.events");
  await extension.unload();
  await monitor.unload();
});

add_task(async function test_windows_event_page() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: "eventpage@windows" } },
      background: { persistent: false },
    },
    background() {
      let removed;
      browser.windows.onCreated.addListener(window => {
        browser.test.sendMessage("onCreated", window.id);
      });
      browser.windows.onRemoved.addListener(wid => {
        removed = wid;
        browser.test.sendMessage("onRemoved", wid);
      });
      browser.windows.onFocusChanged.addListener(wid => {
        if (wid != browser.windows.WINDOW_ID_NONE && wid != removed) {
          browser.test.sendMessage("onFocusChanged", wid);
        }
      });
      browser.test.sendMessage("ready");
    },
  });

  const EVENTS = ["onCreated", "onRemoved", "onFocusChanged"];

  await extension.startup();
  await extension.awaitMessage("ready");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "windows", event, {
      primed: false,
    });
  }

  // test events waken background
  await extension.terminateBackground();
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "windows", event, {
      primed: true,
    });
  }

  let win = await BrowserTestUtils.openNewBrowserWindow();

  await extension.awaitMessage("ready");
  let windowId = await extension.awaitMessage("onCreated");
  ok(true, "persistent event woke background");
  for (let event of EVENTS) {
    assertPersistentListeners(extension, "windows", event, {
      primed: false,
    });
  }
  // focus returns the new window
  let focusedId = await extension.awaitMessage("onFocusChanged");
  Assert.equal(windowId, focusedId, "new window was focused");
  await extension.terminateBackground();

  await BrowserTestUtils.closeWindow(win);
  await extension.awaitMessage("ready");
  let removedId = await extension.awaitMessage("onRemoved");
  Assert.equal(windowId, removedId, "window was removed");
  // focus returns the window focus was passed to
  focusedId = await extension.awaitMessage("onFocusChanged");
  Assert.notEqual(windowId, focusedId, "old window was focused");

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
