"use strict";

function* testHasPermission(params) {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true);

  let contentSetup = params.contentSetup || (() => Promise.resolve());

  let extension = ExtensionTestUtils.loadExtension({
    manifest: params.manifest,

    background: `(${function(contentSetup) {
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.assertEq(msg, "script ran", "script ran");
        browser.test.notifyPass("executeScript");
      });

      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "execute-script");

        browser.tabs.executeScript({
          file: "script.js"
        });
      });

      contentSetup().then(() => {
        browser.test.sendMessage("ready");
      });
    }})(${contentSetup})`,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("script ran");
      }
    }
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  if (params.setup) {
    yield params.setup(extension);
  }

  extension.sendMessage("execute-script");

  yield extension.awaitFinish("executeScript");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
}

// This is a pretty terrible hack, but it's the best we can do until we
// support |executeScript| callbacks and |lastError|.
function* testHasNoPermission(params) {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");

  let contentSetup = params.contentSetup || (() => Promise.resolve());

  let extension = ExtensionTestUtils.loadExtension({
    manifest: params.manifest,

    background: `(${function(contentSetup) {
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.assertEq(msg, "second script ran", "second script ran");
        browser.test.notifyPass("executeScript");
      });

      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "execute-script");

        browser.tabs.query({ activeWindow: true }, tabs => {
          browser.tabs.executeScript({
            file: "script.js"
          });

          // Execute a script we know we have permissions for in the
          // second tab, in the hopes that it will execute after the
          // first one. This has intermittent failure written all over
          // it, but it's just about the best we can do until we
          // support callbacks for executeScript.
          browser.tabs.executeScript(tabs[1].id, {
            file: "second-script.js"
          });
        });
      });

      contentSetup().then(() => {
        browser.test.sendMessage("ready");
      });
    }})(${contentSetup})`,

    files: {
      "script.js": function() {
        browser.runtime.sendMessage("first script ran");
      },

      "second-script.js": function() {
        browser.runtime.sendMessage("second script ran");
      }
    }
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  if (params.setup) {
    yield params.setup(extension);
  }

  extension.sendMessage("execute-script");

  yield extension.awaitFinish("executeScript");
  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab1);
}

add_task(function* testGoodPermissions() {
  info("Test explicit host permission");
  yield testHasPermission({
    manifest: { "permissions": ["http://mochi.test/"] }
  });

  info("Test explicit host subdomain permission");
  yield testHasPermission({
    manifest: { "permissions": ["http://*.mochi.test/"] }
  });

  info("Test explicit <all_urls> permission");
  yield testHasPermission({
    manifest: { "permissions": ["<all_urls>"] }
  });

  info("Test activeTab permission with a browser action click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "browser_action": {},
    },
    setup: clickBrowserAction,
  });

  info("Test activeTab permission with a page action click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "page_action": {},
    },
    contentSetup() {
      return new Promise(resolve => {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          browser.pageAction.show(tabs[0].id);
          resolve();
        });
      });
    },
    setup: clickPageAction,
  });

  info("Test activeTab permission with a browser action w/popup click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "browser_action": { "default_popup": "_blank.html" },
    },
    setup: clickBrowserAction,
  });

  info("Test activeTab permission with a page action w/popup click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab"],
      "page_action": { "default_popup": "_blank.html" },
    },
    contentSetup() {
      return new Promise(resolve => {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          browser.pageAction.show(tabs[0].id);
          resolve();
        });
      });
    },
    setup: clickPageAction,
  });

  info("Test activeTab permission with a context menu click");
  yield testHasPermission({
    manifest: {
      "permissions": ["activeTab", "contextMenus"],
    },
    contentSetup() {
      browser.contextMenus.create({ title: "activeTab", contexts: ["all"] });
      return Promise.resolve();
    },
    setup: function* (extension) {
      info("setup");
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");

      yield BrowserTestUtils.synthesizeMouseAtCenter("a[href]", { type: "contextmenu", button: 2 },
                                                     gBrowser.selectedBrowser);
      info("clicked");
      yield awaitPopupShown;
      info("shown");

      let item = contextMenu.querySelector("[label=activeTab]");

      yield EventUtils.synthesizeMouseAtCenter(item, {}, window);
      info("re-clicked");

      yield awaitPopupHidden;
      info("hidden");
    },
  });
});

add_task(function* testBadPermissions() {
  info("Test no special permissions");
  yield testHasNoPermission({
    manifest: { "permissions": ["http://example.com/"] }
  });

  info("Test tabs permissions");
  yield testHasNoPermission({
    manifest: { "permissions": ["http://example.com/", "tabs"] }
  });

  info("Test active tab, browser action, no click");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/", "activeTab"],
      "browser_action": {},
    },
  });

  info("Test active tab, page action, no click");
  yield testHasNoPermission({
    manifest: {
      "permissions": ["http://example.com/", "activeTab"],
      "page_action": {},
    },
    contentSetup() {
      return new Promise(resolve => {
        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          browser.pageAction.show(tabs[0].id);
          resolve();
        });
      });
    }
  });
});

// TODO: Test that |executeScript| fails if the tab has navigated to a
// new page, and no longer matches our expected state. This involves
// intentionally trying to trigger a race condition, and is probably not
// even worth attempting until we have proper |executeScript| callbacks.
