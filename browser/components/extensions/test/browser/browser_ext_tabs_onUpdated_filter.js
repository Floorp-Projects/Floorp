/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_filter_url() {
  let ext_fail = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.fail(`received unexpected onUpdated event ${JSON.stringify(changeInfo)}`);
      }, {urls: ["*://*.mozilla.org/*"]});
    },
  });
  await ext_fail.startup();

  let ext_perm = ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.fail(`received unexpected onUpdated event without tabs permission`);
      }, {urls: ["*://mochi.test/*"]});
    },
  });
  await ext_perm.startup();

  let ext_ok = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.log(`got onUpdated ${JSON.stringify(changeInfo)}`);
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      }, {urls: ["*://mochi.test/*"]});
    },
  });
  await ext_ok.startup();
  let ok1 = ext_ok.awaitFinish("onUpdated");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await ok1;

  await ext_ok.unload();
  await ext_fail.unload();
  await ext_perm.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_filter_url_activeTab() {
  let ext = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["activeTab"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.fail("should only have notification for activeTab, selectedTab is not activeTab");
      }, {urls: ["*://mochi.test/*"]});
    },
  });
  await ext.startup();

  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      }, {urls: ["*://mochi.test/*"]});
    },
  });
  await ext2.startup();
  let ok = ext2.awaitFinish("onUpdated");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/#foreground");
  await Promise.all([ok]);

  await ext.unload();
  await ext2.unload();
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_filter_tabId() {
  let ext_fail = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.fail(`received unexpected onUpdated event ${JSON.stringify(changeInfo)}`);
      }, {tabId: 12345});
    },
  });
  await ext_fail.startup();

  let ext_ok = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      });
    },
  });
  await ext_ok.startup();
  let ok = ext_ok.awaitFinish("onUpdated");

  let ext_ok2 = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onCreated.addListener(tab => {
        browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
          if (changeInfo.status === "complete") {
            browser.test.notifyPass("onUpdated");
          }
        }, {tabId: tab.id});
        browser.test.log(`Tab specific tab listener on tab ${tab.id}`);
      });
    },
  });
  await ext_ok2.startup();
  let ok2 = ext_ok2.awaitFinish("onUpdated");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await Promise.all([ok, ok2]);

  await ext_ok.unload();
  await ext_ok2.unload();
  await ext_fail.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_filter_windowId() {
  let ext_fail = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.fail(`received unexpected onUpdated event ${JSON.stringify(changeInfo)}`);
      }, {windowId: 12345});
    },
  });
  await ext_fail.startup();

  let ext_ok = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      }, {windowId: browser.windows.WINDOW_ID_CURRENT});
    },
  });
  await ext_ok.startup();
  let ok = ext_ok.awaitFinish("onUpdated");

  let ext_ok2 = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    async background() {
      let window = await browser.windows.getCurrent();
      browser.test.log(`Window specific tab listener on window ${window.id}`);
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      }, {windowId: window.id});
      browser.test.sendMessage("ready");
    },
  });
  await ext_ok2.startup();
  await ext_ok2.awaitMessage("ready");
  let ok2 = ext_ok2.awaitFinish("onUpdated");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await Promise.all([ok, ok2]);

  await ext_ok.unload();
  await ext_ok2.unload();
  await ext_fail.unload();

  await BrowserTestUtils.removeTab(tab);
});

// TODO Bug 1465520 test should be removed when filter name is.
add_task(async function test_filter_isarticle_deprecated() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      // We expect only status updates, anything else is a failure.
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.log(`got onUpdated ${JSON.stringify(changeInfo)}`);
        if ("isArticle" in changeInfo) {
          browser.test.notifyPass("isarticle");
        }
      }, {properties: ["isarticle"]});
    },
  });
  await extension.startup();
  let ok = extension.awaitFinish("isarticle");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await ok;

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_filter_isArticle() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      // We expect only status updates, anything else is a failure.
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.log(`got onUpdated ${JSON.stringify(changeInfo)}`);
        if ("isArticle" in changeInfo) {
          browser.test.notifyPass("isArticle");
        }
      }, {properties: ["isArticle"]});
    },
  });
  await extension.startup();
  let ok = extension.awaitFinish("isArticle");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await ok;

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_filter_property() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },
    background() {
      // We expect only status updates, anything else is a failure.
      let properties = new Set([
        "audible",
        "discarded",
        "favIconUrl",
        "hidden",
        "isArticle",
        "mutedInfo",
        "pinned",
        "sharingState",
        "title",
      ]);
      browser.tabs.onUpdated.addListener((tabId, changeInfo) => {
        browser.test.log(`got onUpdated ${JSON.stringify(changeInfo)}`);
        browser.test.assertTrue(!!changeInfo.status, "changeInfo has status");
        if (Object.keys(changeInfo).some(p => properties.has(p))) {
          browser.test.fail(`received unexpected onUpdated event ${JSON.stringify(changeInfo)}`);
        }
        if (changeInfo.status === "complete") {
          browser.test.notifyPass("onUpdated");
        }
      }, {properties: ["status"]});
    },
  });
  await extension.startup();
  let ok = extension.awaitFinish("onUpdated");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/");
  await ok;

  await extension.unload();

  await BrowserTestUtils.removeTab(tab);
});
