/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_middle_click_with_activeTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_area: "navbar",
      },
      permissions: ["activeTab"],
    },

    background() {
      browser.browserAction.onClicked.addListener(async (tab, info) => {
        browser.test.assertEq(1, info.button, "Expected button value");
        browser.test.assertEq(
          "https://example.com/",
          tab.url,
          "tab.url has the expected url"
        );
        await browser.tabs.insertCSS(tab.id, {
          code: "body { border: 20px solid red; }",
        });
        browser.test.sendMessage("onClick");
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  is(
    ext.tabManager.hasActiveTabPermission(tab),
    false,
    "Active tab was not granted permission"
  );

  await clickBrowserAction(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  is(
    ext.tabManager.hasActiveTabPermission(tab),
    true,
    "Active tab was granted permission"
  );
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_middle_click_with_activeTab_and_popup() {
  const { browserActionFor } = Management.global;

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        browser_style: true,
        default_area: "navbar",
      },
      permissions: ["activeTab"],
    },

    files: {
      "popup.html": `<!DOCTYPE html><html><head><meta charset="utf-8"></head></html>`,
    },

    background() {
      browser.browserAction.onClicked.addListener(async (tab, info) => {
        browser.test.assertEq(1, info.button, "Expected button value");
        browser.test.assertEq(
          "https://example.com/",
          tab.url,
          "tab.url has the expected url"
        );
        await browser.tabs.insertCSS(tab.id, {
          code: "body { border: 20px solid red; }",
        });
        browser.test.sendMessage("onClick");
      });
      browser.test.sendMessage("ready");
    },
  });

  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mouseover" },
    window
  );

  await extension.startup();
  await extension.awaitMessage("ready");

  let widget = getBrowserActionWidget(extension).forWindow(window);
  let ext = WebExtensionPolicy.getByID(extension.id).extension;
  let browserAction = browserActionFor(ext);

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseover" },
    window
  );

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );

  isnot(browserAction.pendingPopup, null, "Have pending popup");
  is(browserAction.action.activeTabForPreload, tab, "Tab to revoke was saved");

  await clickBrowserAction(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  is(
    browserAction.action.activeTabForPreload,
    null,
    "Tab to revoke was removed"
  );

  is(
    browserAction.tabManager.hasActiveTabPermission(tab),
    true,
    "Active tab was granted permission"
  );

  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );

  EventUtils.synthesizeMouseAtCenter(widget.node, { type: "mouseout" }, window);

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_middle_click_without_activeTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/"
  );

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_area: "navbar",
      },
    },

    background() {
      browser.browserAction.onClicked.addListener(async (tab, info) => {
        browser.test.assertEq(1, info.button, "Expected button value");
        browser.test.assertEq(tab.url, undefined, "tab.url is undefined");
        await browser.test.assertRejects(
          browser.tabs.insertCSS(tab.id, {
            code: "body { border: 20px solid red; }",
          }),
          "Missing host permission for the tab",
          "expected failure of tabs.insertCSS without permission"
        );
        browser.test.sendMessage("onClick");
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");

  await clickBrowserAction(extension, window, { button: 1 });
  await extension.awaitMessage("onClick");

  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});
