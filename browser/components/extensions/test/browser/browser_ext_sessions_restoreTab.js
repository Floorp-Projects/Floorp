/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "SessionStore",
                               "resource:///modules/sessionstore/SessionStore.jsm");


/**
 This test checks that after closing an extension made tab it restores correctly.
 The tab is given an expanded triggering principal and we didn't use to serialize
 these correctly into session history.
 */

// Check that we can restore a tab modified by an extension.
add_task(async function test_restoringModifiedTab() {
  function background() {
    browser.tabs.create({url: "http://example.com/"});
    browser.test.onMessage.addListener((msg, filter) => {
      if (msg == "change-tab") {
        browser.tabs.executeScript({code: 'location.href += "?changedTab";'});
      }
    });
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "<all_urls>"],
    },
    "browser_action": {
      "default_title": "Navigate current tab via content script",
    },
    background,
  });

  const contentScriptTabURL = "http://example.com/?changedTab";

  let win = await BrowserTestUtils.openNewBrowserWindow({});

  // Open and close a tabs.
  let tabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, "http://example.com/");
  await extension.startup();
  let firstTab = await tabPromise;
  let locationChange = BrowserTestUtils.waitForLocationChange(win.gBrowser, contentScriptTabURL);
  extension.sendMessage("change-tab");
  await locationChange;
  is(firstTab.linkedBrowser.currentURI.spec, contentScriptTabURL, "Got expected URL");

  let sessionPromise = BrowserTestUtils.waitForSessionStoreUpdate(firstTab);
  BrowserTestUtils.removeTab(firstTab);
  await sessionPromise;

  tabPromise = BrowserTestUtils.waitForNewTab(win.gBrowser, contentScriptTabURL, true);
  SessionStore.undoCloseTab(win, 0);
  let restoredTab = await tabPromise;
  ok(restoredTab, "We returned a tab here");
  is(restoredTab.linkedBrowser.currentURI.spec, contentScriptTabURL, "Got expected URL");

  await extension.unload();
  BrowserTestUtils.removeTab(restoredTab);

  // Close the window.
  await BrowserTestUtils.closeWindow(win);
});
