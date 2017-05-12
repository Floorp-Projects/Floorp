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

add_task(async function test_on_created_navigation_target_from_mouse_click() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("Open link in a new tab using Ctrl-click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter("#test-create-new-tab-from-mouse-click",
                                               {ctrlKey: true, metaKey: true},
                                               tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-tab-from-mouse-click`,
    },
  });

  info("Open link in a new window using Shift-click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter("#test-create-new-window-from-mouse-click",
                                               {shiftKey: true},
                                               tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-window-from-mouse-click`,
    },
  });

  info("Open link with target=\"_blank\" in a new tab using click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter("#test-create-new-tab-from-targetblank-click",
                                               {},
                                               tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: 0,
      url: `${OPENED_PAGE}#new-tab-from-targetblank-click`,
    },
  });

  await BrowserTestUtils.removeTab(tab);

  await extension.unload();
});

add_task(async function test_on_created_navigation_target_from_mouse_click_subframe() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SOURCE_PAGE);

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("Open a subframe link in a new tab using Ctrl-click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter(function() {
        // This code runs as a framescript in the child process and it returns the
        // target link in the subframe.
        return this.content.frames[0].document // eslint-disable-line mozilla/no-cpows-in-tests
          .querySelector("#test-create-new-tab-from-mouse-click-subframe");
      }, {ctrlKey: true, metaKey: true}, tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-tab-from-mouse-click-subframe`,
    },
  });

  info("Open a subframe link in a new window using Shift-click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter(function() {
        // This code runs as a framescript in the child process and it returns the
        // target link in the subframe.
        return this.content.frames[0].document // eslint-disable-line mozilla/no-cpows-in-tests
                      .querySelector("#test-create-new-window-from-mouse-click-subframe");
      }, {shiftKey: true}, tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-window-from-mouse-click-subframe`,
    },
  });

  info("Open a subframe link with target=\"_blank\" in a new tab using click");

  await runTestCase({
    extension,
    openNavTarget() {
      BrowserTestUtils.synthesizeMouseAtCenter(function() {
        // This code runs as a framescript in the child process and it returns the
        // target link in the subframe.
        return this.content.frames[0].document // eslint-disable-line mozilla/no-cpows-in-tests
          .querySelector("#test-create-new-tab-from-targetblank-click-subframe");
      }, {}, tab.linkedBrowser);
    },
    expectedWebNavProps: {
      sourceTabId: expectedSourceTab.sourceTabId,
      sourceFrameId: expectedSourceTab.sourceTabFrames[1].frameId,
      url: `${OPENED_PAGE}#new-tab-from-targetblank-click-subframe`,
    },
  });

  await BrowserTestUtils.removeTab(tab);

  await extension.unload();
});

