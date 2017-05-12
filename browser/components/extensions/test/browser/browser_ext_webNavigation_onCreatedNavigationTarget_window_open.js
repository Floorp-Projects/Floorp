/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const BASE_URL = "http://mochi.test:8888/browser/browser/components/extensions/test/browser";
const SOURCE_PAGE = `${BASE_URL}/webNav_createdTargetSource.html`;
const OPENED_PAGE = `${BASE_URL}/webNav_createdTarget.html`;

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

  browser.test.onMessage.addListener(({type, code}) => {
    if (type === "execute-contentscript") {
      browser.tabs.executeScript(sourceTabId, {code: code});
    }
  });

  browser.test.sendMessage("expectedSourceTab", {
    sourceTabId, sourceTabFrames,
  });
}

async function runTestCase({extension, openNavTarget, expectedWebNavProps}) {
  await openNavTarget();

  const webNavMsg = await extension.awaitMessage("webNavOnCreated");
  const createdTabId = await extension.awaitMessage("tabsOnCreated");
  const completedNavMsg = await extension.awaitMessage("webNavOnCompleted");

  let {sourceTabId, sourceFrameId, url} = expectedWebNavProps;

  is(webNavMsg.tabId, createdTabId, "Got the expected tabId property");
  is(webNavMsg.sourceTabId, sourceTabId, "Got the expected sourceTabId property");
  is(webNavMsg.sourceFrameId, sourceFrameId, "Got the expected sourceFrameId property");
  is(webNavMsg.url, url, "Got the expected url property");

  is(completedNavMsg.tabId, createdTabId, "Got the expected webNavigation.onCompleted tabId property");
  is(completedNavMsg.url, url, "Got the expected webNavigation.onCompleted url property");
}

add_task(async function test_on_created_navigation_target_from_window_open() {
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  gBrowser.selectedTab = tab1;

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation", "tabs", "<all_urls>"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("open an url in a new tab from a window.open call");

  await runTestCase({
    extension,
    openNavTarget() {
      extension.sendMessage({
        type: "execute-contentscript",
        code: `window.open("${OPENED_PAGE}#new-tab-from-window-open"); true;`,
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-tab-from-window-open`,
    },
  });

  info("open an url in a new window from a window.open call");

  await runTestCase({
    extension,
    openNavTarget() {
      extension.sendMessage({
        type: "execute-contentscript",
        code: `window.open("${OPENED_PAGE}#new-win-from-window-open", "_blank", "toolbar=0"); true;`,
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-win-from-window-open`,
    },
  });

  await BrowserTestUtils.removeTab(tab1);

  await extension.unload();
});

add_task(async function test_on_created_navigation_target_from_window_open_subframe() {
  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  gBrowser.selectedTab = tab1;

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation", "tabs", "<all_urls>"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("open an url in a new tab from subframe window.open call");

  await runTestCase({
    extension,
    openNavTarget() {
      extension.sendMessage({
        type: "execute-contentscript",
        code: `document.querySelector('iframe').contentWindow.open("${OPENED_PAGE}#new-tab-from-window-open-subframe"); true;`,
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-tab-from-window-open-subframe`,
    },
  });

  info("open an url in a new window from subframe window.open call");

  await runTestCase({
    extension,
    openNavTarget() {
      extension.sendMessage({
        type: "execute-contentscript",
        code: `document.querySelector('iframe').contentWindow.open("${OPENED_PAGE}#new-win-from-window-open-subframe", "_blank", "toolbar=0"); true;`,
      });
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-win-from-window-open-subframe`,
    },
  });

  await BrowserTestUtils.removeTab(tab1);

  await extension.unload();
});
