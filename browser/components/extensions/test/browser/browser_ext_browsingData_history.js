/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

const OLD_URL = "http://example.com/";
const RECENT_URL = "http://example.com/2/";
const REFERENCE_DATE = new Date();

// pages/visits to add via History.insert
const PAGE_INFOS = [
  {
    url: RECENT_URL,
    title: `test visit for ${RECENT_URL}`,
    visits: [
      {date: REFERENCE_DATE},
    ],
  },
  {
    url: OLD_URL,
    title: `test visit for ${OLD_URL}`,
    visits: [
      {date: new Date(Number(REFERENCE_DATE) - 1000)},
      {date: new Date(Number(REFERENCE_DATE) - 2000)},
    ],
  },
];

async function setupHistory() {
  await PlacesTestUtils.clearHistory();
  await PlacesUtils.history.insertMany(PAGE_INFOS);
  is((await PlacesTestUtils.visitsInDB(RECENT_URL)), 1, "Expected number of visits found in history database.");
  is((await PlacesTestUtils.visitsInDB(OLD_URL)), 2, "Expected number of visits found in history database.");
}

add_task(async function testHistory() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeHistory") {
        await browser.browsingData.removeHistory(options);
      } else {
        await browser.browsingData.remove(options, {history: true});
      }
      browser.test.sendMessage("historyRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear history with no since value.
    await setupHistory();
    extension.sendMessage(method, {});
    await extension.awaitMessage("historyRemoved");

    is((await PlacesTestUtils.visitsInDB(RECENT_URL)), 0, "Expected number of visits removed from history database.");
    is((await PlacesTestUtils.visitsInDB(OLD_URL)), 0, "Expected number of visits removed from history database.");

    // Clear history with recent since value.
    await setupHistory();
    extension.sendMessage(method, {since: REFERENCE_DATE - 1000});
    await extension.awaitMessage("historyRemoved");

    is((await PlacesTestUtils.visitsInDB(RECENT_URL)), 0, "Expected number of visits removed from history database.");
    is((await PlacesTestUtils.visitsInDB(OLD_URL)), 1, "Expected number of visits removed from history database.");

    // Clear history with old since value.
    await setupHistory();
    extension.sendMessage(method, {since: REFERENCE_DATE - 100000});
    await extension.awaitMessage("historyRemoved");

    is((await PlacesTestUtils.visitsInDB(RECENT_URL)), 0, "Expected number of visits removed from history database.");
    is((await PlacesTestUtils.visitsInDB(OLD_URL)), 0, "Expected number of visits removed from history database.");
  }

  await extension.startup();

  await testRemovalMethod("removeHistory");
  await testRemovalMethod("remove");

  await extension.unload();
});
