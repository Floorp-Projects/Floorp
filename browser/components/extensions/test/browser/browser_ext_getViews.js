/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function genericChecker() {
  let kind = window.location.search.slice(1) || "background";
  window.kind = kind;

  let bcGroupId = SpecialPowers.wrap(window).browsingContext.group.id;

  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == kind + "-check-views") {
      let counts = {
        background: 0,
        tab: 0,
        popup: 0,
        kind: 0,
        sidebar: 0,
      };
      if (kind !== "background") {
        counts.kind = browser.extension.getViews({ type: kind }).length;
      }
      let views = browser.extension.getViews();
      let background;
      for (let i = 0; i < views.length; i++) {
        let view = views[i];
        browser.test.assertTrue(view.kind in counts, "view type is valid");
        counts[view.kind]++;
        if (view.kind == "background") {
          browser.test.assertTrue(
            view === browser.extension.getBackgroundPage(),
            "background page is correct"
          );
          background = view;
        }

        browser.test.assertEq(
          bcGroupId,
          SpecialPowers.wrap(view).browsingContext.group.id,
          "browsing context group is correct"
        );
      }
      if (background) {
        browser.runtime.getBackgroundPage().then(view => {
          browser.test.assertEq(
            background,
            view,
            "runtime.getBackgroundPage() is correct"
          );
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
      let url = browser.runtime.getURL("page.html?tab");
      browser.tabs
        .create({ windowId: args[0], url })
        .then(tab => browser.test.sendMessage("opened-tab", tab.id));
    } else if (msg == kind + "-close-tab") {
      browser.tabs.query(
        {
          windowId: args[0],
        },
        tabs => {
          let tab = tabs.find(tab => tab.url.includes("page.html?tab"));
          browser.tabs.remove(tab.id, () => {
            browser.test.sendMessage("closed");
          });
        }
      );
    }
  });
  browser.test.sendMessage(kind + "-ready");
}

async function promiseBrowserContentUnloaded(browser) {
  // Wait until the content has unloaded before resuming the test, to avoid
  // calling extension.getViews too early (and having intermittent failures).
  const MSG_WINDOW_DESTROYED = "Test:BrowserContentDestroyed";
  let unloadPromise = new Promise(resolve => {
    Services.ppmm.addMessageListener(MSG_WINDOW_DESTROYED, function listener() {
      Services.ppmm.removeMessageListener(MSG_WINDOW_DESTROYED, listener);
      resolve();
    });
  });

  await ContentTask.spawn(
    browser,
    MSG_WINDOW_DESTROYED,
    MSG_WINDOW_DESTROYED => {
      let innerWindowId = this.content.windowGlobalChild.innerWindowId;
      let observer = subject => {
        if (
          innerWindowId === subject.QueryInterface(Ci.nsISupportsPRUint64).data
        ) {
          Services.obs.removeObserver(observer, "inner-window-destroyed");

          // Use process message manager to ensure that the message is delivered
          // even after the <browser>'s message manager is disconnected.
          Services.cpmm.sendAsyncMessage(MSG_WINDOW_DESTROYED);
        }
      };
      // Observe inner-window-destroyed, like ExtensionPageChild, to ensure that
      // the ExtensionPageContextChild instance has been unloaded when we resolve
      // the unloadPromise.
      Services.obs.addObserver(observer, "inner-window-destroyed");
    }
  );

  // Return an object so that callers can use "await".
  return { unloadPromise };
}

add_task(async function() {
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary", // To automatically show sidebar on load.
    manifest: {
      permissions: ["tabs"],

      browser_action: {
        default_popup: "page.html?popup",
      },

      sidebar_action: {
        default_panel: "page.html?sidebar",
      },
    },

    files: {
      "page.html": `
      <!DOCTYPE html>
      <html>
      <head><meta charset="utf-8"></head>
      <body>
      <script src="page.js"></script>
      </body></html>
      `,

      "page.js": genericChecker,
    },

    background: genericChecker,
  });

  await Promise.all([
    extension.startup(),
    extension.awaitMessage("background-ready"),
  ]);

  await extension.awaitMessage("sidebar-ready");
  await extension.awaitMessage("sidebar-ready");
  await extension.awaitMessage("sidebar-ready");

  info("started");

  const {
    Management: {
      global: { windowTracker },
    },
  } = ChromeUtils.import("resource://gre/modules/Extension.jsm");

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
    if (kind === "sidebar") {
      // We have 3 sidebars thaat will answer.
      await extension.awaitMessage("counts");
      await extension.awaitMessage("counts");
    }
    is(counts.background, 1, "background count correct");
    is(counts.tab, tabCount, "tab count correct");
    is(counts.popup, popupCount, "popup count correct");
    is(counts.kind, kindCount, "count for type correct");
    is(counts.sidebar, 3, "sidebar count is constant");
  }

  async function checkViewsWithFilter(filter, expectedCount) {
    extension.sendMessage("background-getViews-with-filter", filter);
    let count = await extension.awaitMessage("getViews-count");
    is(count, expectedCount, `count for ${JSON.stringify(filter)} correct`);
  }

  await checkViews("background", 0, 0, 0);
  await checkViews("sidebar", 0, 0, 3);
  await checkViewsWithFilter({ windowId: -1 }, 1);
  await checkViewsWithFilter({ windowId: 0 }, 0);
  await checkViewsWithFilter({ tabId: -1 }, 4);
  await checkViewsWithFilter({ tabId: 0 }, 0);

  let tabId1 = await openTab(winId1);

  await checkViews("background", 1, 0, 0);
  await checkViews("sidebar", 1, 0, 3);
  await checkViews("tab", 1, 0, 1);
  await checkViewsWithFilter({ windowId: winId1 }, 2);
  await checkViewsWithFilter({ tabId: tabId1 }, 1);

  let tabId2 = await openTab(winId2);

  await checkViews("background", 2, 0, 0);
  await checkViews("sidebar", 2, 0, 3);
  await checkViewsWithFilter({ windowId: winId2 }, 2);
  await checkViewsWithFilter({ tabId: tabId2 }, 1);

  async function triggerPopup(win, callback) {
    // Window needs focus to open popups.
    await focusWindow(win);
    await clickBrowserAction(extension, win);
    let browser = await awaitExtensionPanel(extension, win);

    await extension.awaitMessage("popup-ready");

    await callback();

    let { unloadPromise } = await promiseBrowserContentUnloaded(browser);
    closeBrowserAction(extension, win);
    await unloadPromise;
  }

  await triggerPopup(win1, async function() {
    await checkViews("background", 2, 1, 0);
    await checkViews("sidebar", 2, 1, 3);
    await checkViews("popup", 2, 1, 1);
    await checkViewsWithFilter({ windowId: winId1 }, 3);
    await checkViewsWithFilter({ type: "popup", tabId: -1 }, 1);
  });

  await triggerPopup(win2, async function() {
    await checkViews("background", 2, 1, 0);
    await checkViews("sidebar", 2, 1, 3);
    await checkViews("popup", 2, 1, 1);
    await checkViewsWithFilter({ windowId: winId2 }, 3);
    await checkViewsWithFilter({ type: "popup", tabId: -1 }, 1);
  });

  info("checking counts after popups");

  await checkViews("background", 2, 0, 0);
  await checkViews("sidebar", 2, 0, 3);
  await checkViewsWithFilter({ windowId: winId1 }, 2);
  await checkViewsWithFilter({ tabId: -1 }, 4);

  info("closing one tab");

  let { unloadPromise } = await promiseBrowserContentUnloaded(
    win1.gBrowser.selectedBrowser
  );
  extension.sendMessage("background-close-tab", winId1);
  await extension.awaitMessage("closed");
  await unloadPromise;

  info("one tab closed, one remains");

  await checkViews("background", 1, 0, 0);
  await checkViews("sidebar", 1, 0, 3);

  info("opening win1 popup");

  await triggerPopup(win1, async function() {
    await checkViews("background", 1, 1, 0);
    await checkViews("sidebar", 1, 1, 3);
    await checkViews("tab", 1, 1, 1);
    await checkViews("popup", 1, 1, 1);
  });

  info("opening win2 popup");

  await triggerPopup(win2, async function() {
    await checkViews("background", 1, 1, 0);
    await checkViews("sidebar", 1, 1, 3);
    await checkViews("tab", 1, 1, 1);
    await checkViews("popup", 1, 1, 1);
  });

  await checkViews("sidebar", 1, 0, 3);

  await extension.unload();

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function test_getViews_excludes_blocked_parsing_documents() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_popup: "popup.html",
      },
    },
    files: {
      "popup.html": `<!DOCTYPE html>
        <script src="popup.js">
        </script>
        <h1>ExtensionPopup</h1>
      `,
      "popup.js": function() {
        browser.test.sendMessage(
          "browserActionPopup:loaded",
          window.location.href
        );
      },
    },
    background() {
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("getViews", msg, "Got the expected test message");
        const views = browser.extension
          .getViews()
          .map(win => win.location?.href);

        browser.test.sendMessage("getViews:done", views);
      });
      browser.test.sendMessage("bgpage:loaded", window.location.href);
    },
  });

  await extension.startup();
  const bgpageURL = await extension.awaitMessage("bgpage:loaded");
  extension.sendMessage("getViews");
  Assert.deepEqual(
    await extension.awaitMessage("getViews:done"),
    [bgpageURL],
    "Expect only the background page to be initially listed in getViews"
  );

  const {
    Management: {
      global: { browserActionFor },
    },
  } = ChromeUtils.import("resource://gre/modules/Extension.jsm");

  let ext = WebExtensionPolicy.getByID(extension.id)?.extension;
  let browserAction = browserActionFor(ext);

  // Ensure the mouse is not initially hovering the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    window.gURLBar.textbox,
    { type: "mouseover" },
    window
  );

  let widget = await TestUtils.waitForCondition(
    () => getBrowserActionWidget(extension).forWindow(window),
    "Wait for browserAction widget"
  );

  await TestUtils.waitForCondition(
    () => !browserAction.pendingPopup,
    "Wait for no pending preloaded popup"
  );

  await TestUtils.waitForCondition(async () => {
    // Trigger preload browserAction popup (by directly dispatching a MouseEvent
    // to prevent intermittent failures that where often triggered in macos
    // PGO builds when this was using EventUtils.synthesizeMouseAtCenter).
    let mouseOverEvent = new MouseEvent("mouseover");
    widget.node.dispatchEvent(mouseOverEvent);

    await TestUtils.waitForCondition(
      () => browserAction.pendingPopup?.browser,
      "Wait for pending preloaded popup browser"
    );

    return SpecialPowers.spawn(
      browserAction.pendingPopup.browser,
      [],
      async () => {
        const policy = this.content.WebExtensionPolicy.getByHostname(
          this.content.location.hostname
        );
        return policy?.weakExtension
          ?.get()
          ?.blockedParsingDocuments.has(this.content.document);
      }
    ).catch(err => {
      // Tolerate errors triggered by SpecialPowers.spawn
      // being aborted before we got a result back.
      if (err.name === "AbortError") {
        return false;
      }
      throw err;
    });
  }, "Wait for preload browserAction document to be blocked on parsing");

  extension.sendMessage("getViews");
  Assert.deepEqual(
    await extension.awaitMessage("getViews:done"),
    [bgpageURL],
    "Expect preloaded browserAction popup to not be listed in getViews"
  );

  // Test browserAction popup is listed in getViews once document parser is unblocked.
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mousedown", button: 0 },
    window
  );
  EventUtils.synthesizeMouseAtCenter(
    widget.node,
    { type: "mouseup", button: 0 },
    window
  );

  const popupURL = await extension.awaitMessage("browserActionPopup:loaded");

  extension.sendMessage("getViews");
  Assert.deepEqual(
    (await extension.awaitMessage("getViews:done")).sort(),
    [bgpageURL, popupURL].sort(),
    "Expect loaded browserAction popup to be listed in getViews"
  );

  // Ensure the mouse is not hovering the browserAction widget anymore when exiting the test case.
  EventUtils.synthesizeMouseAtCenter(
    window.gURLBar.textbox,
    { type: "mouseover", button: 0 },
    window
  );

  await extension.unload();
});
