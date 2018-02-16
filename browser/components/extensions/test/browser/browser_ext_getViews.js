/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function genericChecker() {
  let kind = "background";
  let path = window.location.pathname;
  if (path.includes("popup")) {
    kind = "popup";
  } else if (path.includes("tab")) {
    kind = "tab";
  }
  window.kind = kind;

  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == kind + "-check-views") {
      let counts = {
        background: 0,
        tab: 0,
        popup: 0,
        kind: 0,
      };
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
    } else if (msg == kind + "-getViews-with-filter") {
      let filter = args[0];
      let count = browser.extension.getViews(filter).length;
      browser.test.sendMessage("getViews-count", count);
    } else if (msg == kind + "-open-tab") {
      browser.tabs.create({windowId: args[0], url: browser.runtime.getURL("tab.html")})
        .then((tab) => browser.test.sendMessage("opened-tab", tab.id));
    } else if (msg == kind + "-close-tab") {
      browser.tabs.query({
        windowId: args[0],
      }, tabs => {
        let tab = tabs.find(tab => tab.url.includes("tab.html"));
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

  await Promise.all([extension.startup(), extension.awaitMessage("background-ready")]);

  info("started");

  let {Management: {global: {windowTracker}}} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

  let winId1 = windowTracker.getId(win1);
  let winId2 = windowTracker.getId(win2);

  async function openTab(winId) {
    extension.sendMessage("background-open-tab", winId);
    await extension.awaitMessage("tab-ready");
    return extension.awaitMessage("opened-tab");
  }

  async function checkViews(kind, tabCount, popupCount, kindCount) {
    extension.sendMessage(kind + "-check-views");
    let counts = await extension.awaitMessage("counts");
    is(counts.background, 1, "background count correct");
    is(counts.tab, tabCount, "tab count correct");
    is(counts.popup, popupCount, "popup count correct");
    is(counts.kind, kindCount, "count for type correct");
  }

  async function checkViewsWithFilter(filter, expectedCount) {
    extension.sendMessage("background-getViews-with-filter", filter);
    let count = await extension.awaitMessage("getViews-count");
    is(count, expectedCount, `count for ${JSON.stringify(filter)} correct`);
  }

  await checkViews("background", 0, 0, 0);
  await checkViewsWithFilter({windowId: -1}, 1);
  await checkViewsWithFilter({tabId: -1}, 1);

  let tabId1 = await openTab(winId1);

  await checkViews("background", 1, 0, 0);
  await checkViews("tab", 1, 0, 1);
  await checkViewsWithFilter({windowId: winId1}, 1);
  await checkViewsWithFilter({tabId: tabId1}, 1);

  let tabId2 = await openTab(winId2);

  await checkViews("background", 2, 0, 0);
  await checkViewsWithFilter({windowId: winId2}, 1);
  await checkViewsWithFilter({tabId: tabId2}, 1);

  async function triggerPopup(win, callback) {
    // Window needs focus to open popups.
    await focusWindow(win);
    await clickBrowserAction(extension, win);
    await awaitExtensionPanel(extension, win);

    await extension.awaitMessage("popup-ready");

    await callback();

    closeBrowserAction(extension, win);
  }

  await triggerPopup(win1, async function() {
    await checkViews("background", 2, 1, 0);
    await checkViews("popup", 2, 1, 1);
    await checkViewsWithFilter({windowId: winId1}, 2);
    await checkViewsWithFilter({type: "popup", tabId: -1}, 1);
  });

  await triggerPopup(win2, async function() {
    await checkViews("background", 2, 1, 0);
    await checkViews("popup", 2, 1, 1);
    await checkViewsWithFilter({windowId: winId2}, 2);
    await checkViewsWithFilter({type: "popup", tabId: -1}, 1);
  });

  info("checking counts after popups");

  await checkViews("background", 2, 0, 0);
  await checkViewsWithFilter({windowId: winId1}, 1);
  await checkViewsWithFilter({tabId: -1}, 1);

  info("closing one tab");

  extension.sendMessage("background-close-tab", winId1);
  await extension.awaitMessage("closed");

  info("one tab closed, one remains");

  await checkViews("background", 1, 0, 0);

  info("opening win1 popup");

  await triggerPopup(win1, async function() {
    await checkViews("background", 1, 1, 0);
    await checkViews("tab", 1, 1, 1);
    await checkViews("popup", 1, 1, 1);
  });

  info("opening win2 popup");

  await triggerPopup(win2, async function() {
    await checkViews("background", 1, 1, 0);
    await checkViews("tab", 1, 1, 1);
    await checkViews("popup", 1, 1, 1);
  });

  await extension.unload();

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});
