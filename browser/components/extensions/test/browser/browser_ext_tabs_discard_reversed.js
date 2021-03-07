/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function tabs_discarded_load_and_discard() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "webNavigation"],
    },
    async background() {
      const PAGE_URL_BEFORE = "http://example.com/initiallyDiscarded";
      const PAGE_URL =
        "http://example.com/browser/browser/components/extensions/test/browser/file_dummy.html";
      // Tabs without titles default to URLs without scheme, according to the
      // logic of tabbrowser.js's setTabTitle/_setTabLabel.
      // TODO bug 1695512: discarded tabs should also follow this logic instead
      // of using the unmodified original URL.
      const PAGE_TITLE_BEFORE = PAGE_URL_BEFORE;
      const PAGE_TITLE_INITIAL = PAGE_URL.replace("http://", "");
      const PAGE_TITLE = "Dummy test page";

      function assertDeepEqual(expected, actual, message) {
        expected = JSON.stringify(expected);
        actual = JSON.stringify(actual);
        browser.test.assertEq(expected, actual, message);
      }

      let tab = await browser.tabs.create({
        url: PAGE_URL_BEFORE,
        discarded: true,
      });
      const TAB_ID = tab.id;
      browser.test.assertTrue(tab.discarded, "Tab initially discarded");
      browser.test.assertEq(PAGE_URL_BEFORE, tab.url, "Initial URL");
      browser.test.assertEq(PAGE_TITLE_BEFORE, tab.title, "Initial title");

      const observedChanges = {
        discarded: [],
        title: [],
        url: [],
      };
      function tabsOnUpdatedAfterLoad(tabId, changeInfo, tab) {
        browser.test.assertEq(TAB_ID, tabId, "tabId for tabs.onUpdated");
        for (let [prop, value] of Object.entries(changeInfo)) {
          observedChanges[prop].push(value);
        }
      }
      browser.tabs.onUpdated.addListener(tabsOnUpdatedAfterLoad, {
        properties: ["discarded", "url", "title"],
      });

      // Load new URL to transition from discarded:true to discarded:false.
      await new Promise(resolve => {
        browser.webNavigation.onCompleted.addListener(details => {
          browser.test.assertEq(TAB_ID, details.tabId, "onCompleted for tab");
          browser.test.assertEq(PAGE_URL, details.url, "URL ater load");
          resolve();
        });
        browser.tabs.update(TAB_ID, { url: PAGE_URL });
      });
      assertDeepEqual(
        [false],
        observedChanges.discarded,
        "changes to tab.discarded after update"
      );
      // TODO bug 1669356: the tabs.onUpdated events should only see the
      // requested URL and its title. However, the current implementation
      // reports several events (including url/title "changes") as part of
      // "restoring" the lazy browser prior to loading the requested URL.
      assertDeepEqual(
        [PAGE_URL_BEFORE, PAGE_URL],
        observedChanges.url,
        "changes to tab.url after update"
      );
      assertDeepEqual(
        [PAGE_TITLE_INITIAL, PAGE_TITLE],
        observedChanges.title,
        "changes to tab.title after update"
      );

      tab = await browser.tabs.get(TAB_ID);
      browser.test.assertFalse(tab.discarded, "tab.discarded after load");
      browser.test.assertEq(PAGE_URL, tab.url, "tab.url after load");
      browser.test.assertEq(PAGE_TITLE, tab.title, "tab.title after load");

      // Reset counters.
      observedChanges.discarded.length = 0;
      observedChanges.title.length = 0;
      observedChanges.url.length = 0;

      // Transition from discarded:false to discarded:true
      await browser.tabs.discard(TAB_ID);
      assertDeepEqual(
        [true],
        observedChanges.discarded,
        "changes to tab.discarded after discard"
      );
      assertDeepEqual([], observedChanges.url, "tab.url not changed");
      assertDeepEqual([], observedChanges.title, "tab.title not changed");

      tab = await browser.tabs.get(TAB_ID);
      browser.test.assertTrue(tab.discarded, "tab.discarded after discard");
      browser.test.assertEq(PAGE_URL, tab.url, "tab.url after discard");
      browser.test.assertEq(PAGE_TITLE, tab.title, "tab.title after discard");

      await browser.tabs.remove(TAB_ID);
      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
