/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

// Basic test to assert that the storage tree and table corresponding to each
// item in the storage tree is correctly displayed

"use strict";

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

// Ensure iframe.src in storage-dfpi.html starts with PREFIX.
const PREFIX = "https://sub1.test1.example";
const ORIGIN = `${PREFIX}.org`;
const ORIGIN_THIRD_PARTY = `${PREFIX}.com`;
const TEST_URL = `${ORIGIN}/${PATH}storage-dfpi.html`;

function listOrigins() {
  return new Promise(resolve => {
    SpecialPowers.Services.qms.listOrigins().callback = req => {
      resolve(req.result);
    };
  });
}

add_task(async function() {
  await pushPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  await pushPref(
    "privacy.partition.always_partition_third_party_non_cookie_storage",
    false
  );

  registerCleanupFunction(SiteDataTestUtils.clear);

  // `Services.qms.listOrigins()` may or contain results created by other tests.
  // And it's unsafe to clear existing origins by `Services.qms.clear()`.
  // In order to obtain correct results, we need to compare the results before
  // and after `openTabAndSetupStorage` is called.
  // To ensure more accurate results, try choosing a uncommon origin for PREFIX.
  const EXISTING_ORIGINS = await listOrigins();
  ok(!EXISTING_ORIGINS.includes(ORIGIN), `${ORIGIN} doesn't exist`);

  await openTabAndSetupStorage(TEST_URL);

  const origins = await listOrigins();
  for (const origin of origins) {
    ok(
      EXISTING_ORIGINS.includes(origin) || origin === ORIGIN,
      `check origin: ${origin}`
    );
  }
  ok(origins.includes(ORIGIN), `${ORIGIN} is added`);

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function setPartitionedStorage(browser, type, key) {
  const handler = async (storageType, storageKey, storageValue) => {
    if (storageType == "cookie") {
      content.document.cookie = `${storageKey}=${storageValue}`;
      return;
    }
    content.localStorage.setItem(storageKey, storageValue);
  };

  // Set first party storage.
  await SpecialPowers.spawn(browser, [type, key, "first"], handler);
  // Set third-party (partitioned) storage in the iframe.
  await SpecialPowers.spawn(
    browser.browsingContext.children[0],
    [type, key, "third"],
    handler
  );
}

async function checkData(storageType, key, value) {
  if (storageType == "cookie") {
    checkCookieData(key, value);
    return;
  }
  await waitForStorageData(key, value);
}

async function testPartitionedStorage(
  storageType,
  treeItemLabel = storageType
) {
  await pushPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );
  // Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
  await pushPref("network.cookie.sameSite.laxByDefault", false);

  info(
    "Open the test url in a new tab and add storage entries *before* opening the storage panel."
  );
  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    await setPartitionedStorage(browser, storageType, "contextA");
  });

  await openTabAndSetupStorage(TEST_URL);

  const doc = gPanelWindow.document;

  info("check that both hosts appear in the storage tree");
  checkTree(doc, [treeItemLabel, ORIGIN]);
  checkTree(doc, [treeItemLabel, ORIGIN_THIRD_PARTY]);

  info(
    "check that items for both first and third party host have the initial storage entries"
  );

  await selectTreeItem([treeItemLabel, ORIGIN]);
  await checkData(storageType, "contextA", "first");

  await selectTreeItem([treeItemLabel, ORIGIN_THIRD_PARTY]);
  await checkData(storageType, "contextA", "third");

  info("Add more entries while the storage panel is open");
  const onUpdated = gUI.once("store-objects-edit");
  await setPartitionedStorage(
    gBrowser.selectedBrowser,
    storageType,
    "contextB"
  );
  await onUpdated;

  info("check that both hosts appear in the storage tree");
  checkTree(doc, [treeItemLabel, ORIGIN]);
  checkTree(doc, [treeItemLabel, ORIGIN_THIRD_PARTY]);

  info(
    "check that items for both first and third party host have the updated storage entries"
  );

  await selectTreeItem([treeItemLabel, ORIGIN]);
  await checkData(storageType, "contextA", "first");
  await checkData(storageType, "contextB", "first");

  await selectTreeItem([treeItemLabel, ORIGIN_THIRD_PARTY]);
  await checkData(storageType, "contextA", "third");
  await checkData(storageType, "contextB", "third");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

// Tests that partitioned storage is shown in the storage panel.

add_task(async function test_partitioned_cookies() {
  registerCleanupFunction(SiteDataTestUtils.clear);
  await testPartitionedStorage("cookie", "cookies");
});

add_task(async function test_partitioned_localStorage() {
  registerCleanupFunction(SiteDataTestUtils.clear);
  await testPartitionedStorage("localStorage");
});
