/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

const OLD_URL = "http://example.com/";
const RECENT_URL = "http://example.com/2/";
const REFERENCE_DATE = new Date();

// Visits to add via addVisits
const PLACEINFO = [
  {
    uri: RECENT_URL,
    title: `test visit for ${RECENT_URL}`,
    visitDate: REFERENCE_DATE,
  },
  {
    uri: OLD_URL,
    title: `test visit for ${OLD_URL}`,
    visitDate: new Date(Number(REFERENCE_DATE) - 1000),
  },
  {
    uri: OLD_URL,
    title: `test visit for ${OLD_URL}`,
    visitDate: new Date(Number(REFERENCE_DATE) - 2000),
  },
];

async function setupHistory() {
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits(PLACEINFO);
  is(
    await PlacesTestUtils.visitsInDB(RECENT_URL),
    1,
    "Expected number of visits found in history database."
  );
  is(
    await PlacesTestUtils.visitsInDB(OLD_URL),
    2,
    "Expected number of visits found in history database."
  );
}

add_task(async function testHistory() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeHistory") {
        await browser.browsingData.removeHistory(options);
      } else {
        await browser.browsingData.remove(options, { history: true });
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

    is(
      await PlacesTestUtils.visitsInDB(RECENT_URL),
      0,
      "Expected number of visits removed from history database."
    );
    is(
      await PlacesTestUtils.visitsInDB(OLD_URL),
      0,
      "Expected number of visits removed from history database."
    );

    // Clear history with recent since value.
    await setupHistory();
    extension.sendMessage(method, { since: REFERENCE_DATE - 1000 });
    await extension.awaitMessage("historyRemoved");

    is(
      await PlacesTestUtils.visitsInDB(RECENT_URL),
      0,
      "Expected number of visits removed from history database."
    );
    is(
      await PlacesTestUtils.visitsInDB(OLD_URL),
      1,
      "Expected number of visits removed from history database."
    );

    // Clear history with old since value.
    await setupHistory();
    extension.sendMessage(method, { since: REFERENCE_DATE - 100000 });
    await extension.awaitMessage("historyRemoved");

    is(
      await PlacesTestUtils.visitsInDB(RECENT_URL),
      0,
      "Expected number of visits removed from history database."
    );
    is(
      await PlacesTestUtils.visitsInDB(OLD_URL),
      0,
      "Expected number of visits removed from history database."
    );
  }

  await extension.startup();

  await testRemovalMethod("removeHistory");
  await testRemovalMethod("remove");

  await extension.unload();
});
