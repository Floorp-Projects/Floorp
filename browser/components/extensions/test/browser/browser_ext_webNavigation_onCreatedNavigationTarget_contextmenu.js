/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(new URL("head_webNavigation.js", gTestPath).href,
                                    this);

SpecialPowers.pushPrefEnv({"set": [["security.allow_eval_with_system_principal",
                                    true]]});

async function clickContextMenuItem({pageElementSelector, contextMenuItemLabel}) {
  const contentAreaContextMenu = await openContextMenu(pageElementSelector);
  const item = contentAreaContextMenu.getElementsByAttribute("label", contextMenuItemLabel);
  is(item.length, 1, `found contextMenu item for "${contextMenuItemLabel}"`);
  item[0].click();
  await closeContextMenu();
}

async function background() {
  const tabs = await browser.tabs.query({active: true, currentWindow: true});
  const sourceTabId = tabs[0].id;

  const sourceTabFrames = await browser.webNavigation.getAllFrames({tabId: sourceTabId});

  browser.webNavigation.onCreatedNavigationTarget.addListener((msg) => {
    browser.test.sendMessage("webNavOnCreated", msg);
  });

  browser.webNavigation.onCompleted.addListener(async (msg) => {
    // NOTE: checking the url is currently necessary because of Bug 1252129
    // ( Filter out webNavigation events related to new window initialization phase).
    if (msg.tabId !== sourceTabId && msg.url !== "about:blank") {
      await browser.tabs.remove(msg.tabId);
      browser.test.sendMessage("webNavOnCompleted", msg);
    }
  });

  browser.tabs.onCreated.addListener((tab) => {
    browser.test.sendMessage("tabsOnCreated", tab.id);
  });

  browser.test.sendMessage("expectedSourceTab", {
    sourceTabId, sourceTabFrames,
  });
}

add_task(async function test_on_created_navigation_target_from_context_menu() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("Open link in a new tab from the context menu");

  await runCreatedNavigationTargetTest({
    extension,
    async openNavTarget() {
      await clickContextMenuItem({
        pageElementSelector: "#test-create-new-tab-from-context-menu",
        contextMenuItemLabel: "Open Link in New Tab",
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-tab-from-context-menu`,
    },
  });

  info("Open link in a new window from the context menu");

  await runCreatedNavigationTargetTest({
    extension,
    async openNavTarget() {
      await clickContextMenuItem({
        pageElementSelector: "#test-create-new-window-from-context-menu",
        contextMenuItemLabel: "Open Link in New Window",
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-window-from-context-menu`,
    },
  });

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
});

add_task(async function test_on_created_navigation_target_from_context_menu_subframe() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("Open a subframe link in a new tab from the context menu");

  await runCreatedNavigationTargetTest({
    extension,
    async openNavTarget() {
      await clickContextMenuItem({
        pageElementSelector: () => {
          // This code runs as a framescript in the child process and it returns the
          // target link in the subframe.
          return this.content.frames[0]
            .document.querySelector("#test-create-new-tab-from-context-menu-subframe");
        },
        contextMenuItemLabel: "Open Link in New Tab",
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-tab-from-context-menu-subframe`,
    },
  });

  info("Open a subframe link in a new window from the context menu");

  await runCreatedNavigationTargetTest({
    extension,
    async openNavTarget() {
      await clickContextMenuItem({
        pageElementSelector: () => {
          // This code runs as a framescript in the child process and it returns the
          // target link in the subframe.
          return this.content.frames[0]
            .document.querySelector("#test-create-new-window-from-context-menu-subframe");
        },
        contextMenuItemLabel: "Open Link in New Window",
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-window-from-context-menu-subframe`,
    },
  });

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
});
