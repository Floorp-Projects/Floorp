/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function genericChecker() {
  let kind = "background";
  let path = window.location.pathname;
  if (path.indexOf("popup") != -1) {
    kind = "popup";
  } else if (path.indexOf("tab") != -1) {
    kind = "tab";
  }
  window.kind = kind;

  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == kind + "-check-views") {
      let windowId = args[0];
      let counts = {
        "background": 0,
        "tab": 0,
        "popup": 0,
        "kind": 0,
        "window": 0,
      };
      if (Number.isInteger(windowId)) {
        counts.window = browser.extension.getViews({windowId: windowId}).length;
      }
      if (kind !== "background") {
        counts.kind = browser.extension.getViews({type: kind}).length;
      }
      let views = browser.extension.getViews();
      let background;
      for (let i = 0; i < views.length; i++) {
        let view = views[i];
        browser.test.assertTrue(view.kind in counts, "view type is valid");
        counts[view.kind]++;
        if (view.kind == "background") {
          browser.test.assertTrue(view === browser.extension.getBackgroundPage(),
                                  "background page is correct");
          background = view;
        }
      }
      if (background) {
        browser.runtime.getBackgroundPage().then(view => {
          browser.test.assertEq(background, view, "runtime.getBackgroundPage() is correct");
          browser.test.sendMessage("counts", counts);
        });
      } else {
        browser.test.sendMessage("counts", counts);
      }
    } else if (msg == kind + "-open-tab") {
      browser.tabs.create({windowId: args[0], url: browser.runtime.getURL("tab.html")});
    } else if (msg == kind + "-close-tab") {
      browser.tabs.query({
        windowId: args[0],
      }, tabs => {
        let tab = tabs.find(tab => tab.url.indexOf("tab.html") != -1);
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

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "browser_action": {
        "default_popup": "popup.html",
      },
    },

    files: {
      "tab.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="tab.js"></script>
      </body></html>
      `,

      "tab.js": genericChecker,

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

  info("started");

  let {Management: {global: {WindowManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let winId1 = WindowManager.getId(win1);
  let winId2 = WindowManager.getId(win2);

  function* openTab(winId) {
    extension.sendMessage("background-open-tab", winId);
    yield extension.awaitMessage("tab-ready");
  }

  function* checkViews(kind, tabCount, popupCount, kindCount, windowId = undefined, windowCount = 0) {
    extension.sendMessage(kind + "-check-views", windowId);
    let counts = yield extension.awaitMessage("counts");
    is(counts.background, 1, "background count correct");
    is(counts.tab, tabCount, "tab count correct");
    is(counts.popup, popupCount, "popup count correct");
    is(counts.kind, kindCount, "count for type correct");
    is(counts.window, windowCount, "count for window correct");
  }

  yield checkViews("background", 0, 0, 0);

  yield openTab(winId1);

  yield checkViews("background", 1, 0, 0, winId1, 1);
  yield checkViews("tab", 1, 0, 1);

  yield openTab(winId2);

  yield checkViews("background", 2, 0, 0, winId2, 1);

  function* triggerPopup(win, callback) {
    yield clickBrowserAction(extension, win);
    yield awaitExtensionPanel(extension, win);

    yield extension.awaitMessage("popup-ready");

    yield callback();

    closeBrowserAction(extension, win);
  }

  // The popup occasionally closes prematurely if we open it immediately here.
  // I'm not sure what causes it to close (it's something internal, and seems to
  // be focus-related, but it's not caused by JS calling hidePopup), but even a
  // short timeout seems to consistently fix it.
  yield new Promise(resolve => win1.setTimeout(resolve, 10));

  yield triggerPopup(win1, function* () {
    yield checkViews("background", 2, 1, 0, winId1, 2);
    yield checkViews("popup", 2, 1, 1);
  });

  yield triggerPopup(win2, function* () {
    yield checkViews("background", 2, 1, 0, winId2, 2);
    yield checkViews("popup", 2, 1, 1);
  });

  info("checking counts after popups");

  yield checkViews("background", 2, 0, 0, winId1, 1);

  info("closing one tab");

  extension.sendMessage("background-close-tab", winId1);
  yield extension.awaitMessage("closed");

  info("one tab closed, one remains");

  yield checkViews("background", 1, 0, 0);

  info("opening win1 popup");

  yield triggerPopup(win1, function* () {
    yield checkViews("background", 1, 1, 0);
    yield checkViews("tab", 1, 1, 1);
    yield checkViews("popup", 1, 1, 1);
  });

  info("opening win2 popup");

  yield triggerPopup(win2, function* () {
    yield checkViews("background", 1, 1, 0);
    yield checkViews("tab", 1, 1, 1);
    yield checkViews("popup", 1, 1, 1);
  });

  yield extension.unload();

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});
