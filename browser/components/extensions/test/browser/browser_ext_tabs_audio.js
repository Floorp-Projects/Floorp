/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank?1");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank?2");

  gBrowser.selectedTab = tab1;

  async function background() {
    function promiseUpdated(tabId, attr) {
      return new Promise(resolve => {
        let onUpdated = (tabId_, changeInfo, tab) => {
          if (tabId == tabId_ && attr in changeInfo) {
            browser.tabs.onUpdated.removeListener(onUpdated);

            resolve({changeInfo, tab});
          }
        };
        browser.tabs.onUpdated.addListener(onUpdated);
      });
    }

    let deferred = {};
    browser.test.onMessage.addListener((message, tabId, result) => {
      if (message == "change-tab-done" && deferred[tabId]) {
        deferred[tabId].resolve(result);
      }
    });

    function changeTab(tabId, attr, on) {
      return new Promise((resolve, reject) => {
        deferred[tabId] = {resolve, reject};
        browser.test.sendMessage("change-tab", tabId, attr, on);
      });
    }


    try {
      let tabs = await browser.tabs.query({lastFocusedWindow: true});
      browser.test.assertEq(tabs.length, 3, "We have three tabs");

      for (let tab of tabs) {
        // Note: We want to check that these are actual boolean values, not
        // just that they evaluate as false.
        browser.test.assertEq(false, tab.mutedInfo.muted, "Tab is not muted");
        browser.test.assertEq(undefined, tab.mutedInfo.reason, "Tab has no muted info reason");
        browser.test.assertEq(false, tab.audible, "Tab is not audible");
      }

      let windowId = tabs[0].windowId;
      let tabIds = [tabs[1].id, tabs[2].id];

      browser.test.log("Test initial queries for muted and audible return no tabs");
      let silent = await browser.tabs.query({windowId, audible: false});
      let audible = await browser.tabs.query({windowId, audible: true});
      let muted = await browser.tabs.query({windowId, muted: true});
      let nonMuted = await browser.tabs.query({windowId, muted: false});

      browser.test.assertEq(3, silent.length, "Three silent tabs");
      browser.test.assertEq(0, audible.length, "No audible tabs");

      browser.test.assertEq(0, muted.length, "No muted tabs");
      browser.test.assertEq(3, nonMuted.length, "Three non-muted tabs");

      browser.test.log("Toggle muted and audible externally on one tab each, and check results");
      [muted, audible] = await Promise.all([
        promiseUpdated(tabIds[0], "mutedInfo"),
        promiseUpdated(tabIds[1], "audible"),
        changeTab(tabIds[0], "muted", true),
        changeTab(tabIds[1], "audible", true),
      ]);

      for (let obj of [muted.changeInfo, muted.tab]) {
        browser.test.assertEq(true, obj.mutedInfo.muted, "Tab is muted");
        browser.test.assertEq("user", obj.mutedInfo.reason, "Tab was muted by the user");
      }

      browser.test.assertEq(true, audible.changeInfo.audible, "Tab audible state changed");
      browser.test.assertEq(true, audible.tab.audible, "Tab is audible");

      browser.test.log("Re-check queries. Expect one audible and one muted tab");
      silent = await browser.tabs.query({windowId, audible: false});
      audible = await browser.tabs.query({windowId, audible: true});
      muted = await browser.tabs.query({windowId, muted: true});
      nonMuted = await browser.tabs.query({windowId, muted: false});

      browser.test.assertEq(2, silent.length, "Two silent tabs");
      browser.test.assertEq(1, audible.length, "One audible tab");

      browser.test.assertEq(1, muted.length, "One muted tab");
      browser.test.assertEq(2, nonMuted.length, "Two non-muted tabs");

      browser.test.assertEq(true, muted[0].mutedInfo.muted, "Tab is muted");
      browser.test.assertEq("user", muted[0].mutedInfo.reason, "Tab was muted by the user");

      browser.test.assertEq(true, audible[0].audible, "Tab is audible");

      browser.test.log("Toggle muted internally on two tabs, and check results");
      [nonMuted, muted] = await Promise.all([
        promiseUpdated(tabIds[0], "mutedInfo"),
        promiseUpdated(tabIds[1], "mutedInfo"),
        browser.tabs.update(tabIds[0], {muted: false}),
        browser.tabs.update(tabIds[1], {muted: true}),
      ]);

      for (let obj of [nonMuted.changeInfo, nonMuted.tab]) {
        browser.test.assertEq(false, obj.mutedInfo.muted, "Tab is not muted");
      }
      for (let obj of [muted.changeInfo, muted.tab]) {
        browser.test.assertEq(true, obj.mutedInfo.muted, "Tab is muted");
      }

      for (let obj of [nonMuted.changeInfo, nonMuted.tab, muted.changeInfo, muted.tab]) {
        browser.test.assertEq("extension", obj.mutedInfo.reason, "Mute state changed by extension");

        // FIXME: browser.runtime.id is currently broken.
        browser.test.assertEq(browser.i18n.getMessage("@@extension_id"),
                              obj.mutedInfo.extensionId,
                              "Mute state changed by extension");
      }

      browser.test.log("Test that mutedInfo is preserved by sessionstore");
      let tab = await changeTab(tabIds[1], "duplicate").then(browser.tabs.get);

      browser.test.assertEq(true, tab.mutedInfo.muted, "Tab is muted");

      browser.test.assertEq("extension", tab.mutedInfo.reason, "Mute state changed by extension");

      // FIXME: browser.runtime.id is currently broken.
      browser.test.assertEq(browser.i18n.getMessage("@@extension_id"),
                            tab.mutedInfo.extensionId,
                            "Mute state changed by extension");

      browser.test.log("Unmute externally, and check results");
      [nonMuted] = await Promise.all([
        promiseUpdated(tabIds[1], "mutedInfo"),
        changeTab(tabIds[1], "muted", false),
        browser.tabs.remove(tab.id),
      ]);

      for (let obj of [nonMuted.changeInfo, nonMuted.tab]) {
        browser.test.assertEq(false, obj.mutedInfo.muted, "Tab is not muted");
        browser.test.assertEq("user", obj.mutedInfo.reason, "Mute state changed by user");
      }

      browser.test.notifyPass("tab-audio");
    } catch (e) {
      browser.test.fail(`${e} :: ${e.stack}`);
      browser.test.notifyFail("tab-audio");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  extension.onMessage("change-tab", (tabId, attr, on) => {
    let {Management: {global: {tabTracker}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

    let tab = tabTracker.getTab(tabId);

    if (attr == "muted") {
      // Ideally we'd simulate a click on the tab audio icon for this, but the
      // handler relies on CSS :hover states, which are complicated and fragile
      // to simulate.
      if (tab.muted != on) {
        tab.toggleMuteAudio();
      }
    } else if (attr == "audible") {
      let browser = tab.linkedBrowser;
      if (on) {
        browser.audioPlaybackStarted();
      } else {
        browser.audioPlaybackStopped();
      }
    } else if (attr == "duplicate") {
      // This is a bit of a hack. It won't be necessary once we have
      // `tabs.duplicate`.
      let newTab = gBrowser.duplicateTab(tab);
      BrowserTestUtils.waitForEvent(newTab, "SSTabRestored", () => true).then(() => {
        extension.sendMessage("change-tab-done", tabId, tabTracker.getId(newTab));
      });
      return;
    }

    extension.sendMessage("change-tab-done", tabId);
  });

  yield extension.startup();

  yield extension.awaitFinish("tab-audio");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
