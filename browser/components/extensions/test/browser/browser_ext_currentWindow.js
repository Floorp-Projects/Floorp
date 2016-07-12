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

add_task(function* () {
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();

  yield focusWindow(win2);

  yield BrowserTestUtils.loadURI(win1.gBrowser.selectedBrowser, "about:robots");
  yield BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  yield BrowserTestUtils.loadURI(win2.gBrowser.selectedBrowser, "about:config");
  yield BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);

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

  yield Promise.all([extension.startup(), extension.awaitMessage("background-ready")]);

  let {WindowManager} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let winId1 = WindowManager.getId(win1);
  let winId2 = WindowManager.getId(win2);

  function* checkWindow(kind, winId, name) {
    extension.sendMessage(kind + "-check-current1");
    is((yield extension.awaitMessage("result")), winId, `${name} is on top (check 1) [${kind}]`);
    extension.sendMessage(kind + "-check-current2");
    is((yield extension.awaitMessage("result")), winId, `${name} is on top (check 2) [${kind}]`);
    extension.sendMessage(kind + "-check-current3");
    is((yield extension.awaitMessage("result")), winId, `${name} is on top (check 3) [${kind}]`);
  }

  yield focusWindow(win1);
  yield checkWindow("background", winId1, "win1");
  yield focusWindow(win2);
  yield checkWindow("background", winId2, "win2");

  function* triggerPopup(win, callback) {
    yield clickBrowserAction(extension, win);

    yield extension.awaitMessage("popup-ready");

    yield callback();

    closeBrowserAction(extension, win);
  }

  // Set focus to some other window.
  yield focusWindow(window);

  yield triggerPopup(win1, function* () {
    yield checkWindow("popup", winId1, "win1");
  });

  yield triggerPopup(win2, function* () {
    yield checkWindow("popup", winId2, "win2");
  });

  function* triggerPage(winId, name) {
    extension.sendMessage("background-open-page", winId);
    yield extension.awaitMessage("page-ready");
    yield checkWindow("page", winId, name);
    extension.sendMessage("background-close-page", winId);
    yield extension.awaitMessage("closed");
  }

  yield triggerPage(winId1, "win1");
  yield triggerPage(winId2, "win2");

  yield extension.unload();

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});
