/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank?1");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank?2");

  gBrowser.selectedTab = tab1;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background() {
      let activeTab;
      let tabId;
      let tabIds;
      browser.tabs.query({lastFocusedWindow: true}).then(tabs => {
        browser.test.assertEq(3, tabs.length, "We have three tabs");

        browser.test.assertTrue(tabs[1].active, "Tab 1 is active");
        activeTab = tabs[1];

        tabIds = tabs.map(tab => tab.id);

        return browser.tabs.create({openerTabId: activeTab.id, active: false});
      }).then(tab => {
        browser.test.assertEq(activeTab.id, tab.openerTabId, "Tab opener ID is correct");
        browser.test.assertEq(activeTab.index + 1, tab.index, "Tab was inserted after the related current tab");

        tabId = tab.id;
        return browser.tabs.get(tabId);
      }).then(tab => {
        browser.test.assertEq(activeTab.id, tab.openerTabId, "Tab opener ID is still correct");

        return browser.tabs.update(tabId, {openerTabId: tabIds[0]});
      }).then(tab => {
        browser.test.assertEq(tabIds[0], tab.openerTabId, "Updated tab opener ID is correct");

        return browser.tabs.get(tabId);
      }).then(tab => {
        browser.test.assertEq(tabIds[0], tab.openerTabId, "Updated tab opener ID is still correct");

        return browser.tabs.create({openerTabId: tabId, active: false});
      }).then(tab => {
        browser.test.assertEq(tabId, tab.openerTabId, "New tab opener ID is correct");
        browser.test.assertEq(tabIds.length, tab.index, "New tab was not inserted after the unrelated current tab");

        let promise = browser.tabs.remove(tabId);

        tabId = tab.id;
        return promise;
      }).then(() => {
        return browser.tabs.get(tabId);
      }).then(tab => {
        browser.test.assertEq(undefined, tab.openerTabId, "Tab opener ID was cleared after opener tab closed");

        return browser.tabs.remove(tabId);
      }).then(() => {
        browser.test.notifyPass("tab-opener");
      }).catch(e => {
        browser.test.fail(`${e} :: ${e.stack}`);
        browser.test.notifyFail("tab-opener");
      });
    },
  });

  await extension.startup();

  await extension.awaitFinish("tab-opener");

  await extension.unload();

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
});
