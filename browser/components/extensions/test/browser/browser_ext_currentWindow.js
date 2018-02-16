/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function genericChecker() {
  let kind = "background";
  let path = window.location.pathname;
  if (path.includes("/popup.html")) {
    kind = "popup";
  } else if (path.includes("/page.html")) {
    kind = "page";
  }

  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == kind + "-check-current1") {
      browser.tabs.query({
        currentWindow: true,
      }, function(tabs) {
        browser.test.sendMessage("result", tabs[0].windowId);
      });
    } else if (msg == kind + "-check-current2") {
      browser.tabs.query({
        windowId: browser.windows.WINDOW_ID_CURRENT,
      }, function(tabs) {
        browser.test.sendMessage("result", tabs[0].windowId);
      });
    } else if (msg == kind + "-check-current3") {
      browser.windows.getCurrent(function(window) {
        browser.test.sendMessage("result", window.id);
      });
    } else if (msg == kind + "-open-page") {
      browser.tabs.create({windowId: args[0], url: browser.runtime.getURL("page.html")});
    } else if (msg == kind + "-close-page") {
      browser.tabs.query({
        windowId: args[0],
      }, tabs => {
        let tab = tabs.find(tab => tab.url.includes("/page.html"));
        browser.tabs.remove(tab.id, () => {
          browser.test.sendMessage("closed");
        });
      });
    }
  });
  browser.test.sendMessage(kind + "-ready");
}

add_task(async function() {
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  await focusWindow(win2);

  await BrowserTestUtils.loadURI(win1.gBrowser.selectedBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  await BrowserTestUtils.loadURI(win2.gBrowser.selectedBrowser, "about:config");
  await BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "browser_action": {
        "default_popup": "popup.html",
      },
    },

    files: {
      "page.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="page.js"></script>
      </body></html>
      `,

      "page.js": genericChecker,

      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": genericChecker,
    },

    background: genericChecker,
  });

  await Promise.all([extension.startup(), extension.awaitMessage("background-ready")]);

  let {Management: {global: {windowTracker}}} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

  let winId1 = windowTracker.getId(win1);
  let winId2 = windowTracker.getId(win2);

  async function checkWindow(kind, winId, name) {
    extension.sendMessage(kind + "-check-current1");
    is((await extension.awaitMessage("result")), winId, `${name} is on top (check 1) [${kind}]`);
    extension.sendMessage(kind + "-check-current2");
    is((await extension.awaitMessage("result")), winId, `${name} is on top (check 2) [${kind}]`);
    extension.sendMessage(kind + "-check-current3");
    is((await extension.awaitMessage("result")), winId, `${name} is on top (check 3) [${kind}]`);
  }

  async function triggerPopup(win, callback) {
    await clickBrowserAction(extension, win);
    await awaitExtensionPanel(extension, win);

    await extension.awaitMessage("popup-ready");

    await callback();

    closeBrowserAction(extension, win);
  }

  await focusWindow(win1);
  await checkWindow("background", winId1, "win1");
  await triggerPopup(win1, async function() {
    await checkWindow("popup", winId1, "win1");
  });

  await focusWindow(win2);
  await checkWindow("background", winId2, "win2");
  await triggerPopup(win2, async function() {
    await checkWindow("popup", winId2, "win2");
  });

  async function triggerPage(winId, name) {
    extension.sendMessage("background-open-page", winId);
    await extension.awaitMessage("page-ready");
    await checkWindow("page", winId, name);
    extension.sendMessage("background-close-page", winId);
    await extension.awaitMessage("closed");
  }

  await triggerPage(winId1, "win1");
  await triggerPage(winId2, "win2");

  await extension.unload();

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
