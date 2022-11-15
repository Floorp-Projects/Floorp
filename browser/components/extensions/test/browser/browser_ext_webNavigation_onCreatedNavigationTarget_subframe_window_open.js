/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

loadTestSubscript("head_webNavigation.js");

async function background() {
  const tabs = await browser.tabs.query({ active: true, currentWindow: true });
  const sourceTabId = tabs[0].id;

  const sourceTabFrames = await browser.webNavigation.getAllFrames({
    tabId: sourceTabId,
  });

  browser.webNavigation.onCreatedNavigationTarget.addListener(msg => {
    browser.test.sendMessage("webNavOnCreated", msg);
  });

  browser.webNavigation.onCompleted.addListener(async msg => {
    // NOTE: checking the url is currently necessary because of Bug 1252129
    // ( Filter out webNavigation events related to new window initialization phase).
    if (msg.tabId !== sourceTabId && msg.url !== "about:blank") {
      await browser.tabs.remove(msg.tabId);
      browser.test.sendMessage("webNavOnCompleted", msg);
    }
  });

  browser.tabs.onCreated.addListener(tab => {
    browser.test.sendMessage("tabsOnCreated", tab.id);
  });

  browser.test.onMessage.addListener(({ type, code }) => {
    if (type === "execute-contentscript") {
      browser.tabs.executeScript(sourceTabId, { code: code });
    }
  });

  browser.test.sendMessage("expectedSourceTab", {
    sourceTabId,
    sourceTabFrames,
  });
}

add_task(async function test_window_open_from_subframe() {
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SOURCE_PAGE
  );

  gBrowser.selectedTab = tab1;

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["webNavigation", "tabs", "<all_urls>"],
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  info("open a url in a new tab from subframe window.open call");

  await runCreatedNavigationTargetTest({
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

  info("open a url in a new window from subframe window.open call");

  await runCreatedNavigationTargetTest({
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

  BrowserTestUtils.removeTab(tab1);

  await extension.unload();
});

add_task(async function test_window_open_close_from_browserAction_popup() {
  const tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    SOURCE_PAGE
  );

  gBrowser.selectedTab = tab1;

  function popup() {
    window.open("", "_self").close();

    browser.test.sendMessage("browserAction_popup_executed");
  }

  const extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      browser_action: {
        default_popup: "popup.html",
        default_area: "navbar",
      },
      permissions: ["webNavigation", "tabs", "<all_urls>"],
    },
    files: {
      "popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <script src="popup.js"></script>
          </body>
        </html>
      `,
      "popup.js": popup,
    },
  });

  await extension.startup();

  const expectedSourceTab = await extension.awaitMessage("expectedSourceTab");

  clickBrowserAction(extension);

  await extension.awaitMessage("browserAction_popup_executed");

  info("open a url in a new tab from a window.open call");

  await runCreatedNavigationTargetTest({
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

  BrowserTestUtils.removeTab(tab1);

  await extension.unload();
});
