/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

// Basic test to assert that the storage tree and table corresponding to each
// item in the storage tree is correctly displayed, bearing in mind the origin
// is partitioned when always_partition_third_party_non_cookie_storage is true.

"use strict";

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

// Ensure iframe.src in storage-dfpi.html starts with PREFIX.
const PREFIX = "https://sub1.test1.example";
const ORIGIN = `${PREFIX}.org`;
const ORIGIN_PARTITIONED = `${PREFIX}.com^partitionKey=%28https%2Cexample.org%29`;
const TEST_URL = `${ORIGIN}/document-builder.sjs?html=
    <iframe src="${PREFIX}.com/browser/devtools/client/storage/test/storage-blank.html"></iframe>
`;

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
    true
  );

  registerCleanupFunction(SiteDataTestUtils.clear);

  const expectedOrigins = [ORIGIN, ORIGIN_PARTITIONED];

  // `Services.qms.listOrigins()` may or contain results created by other tests.
  // And it's unsafe to clear existing origins by `Services.qms.clear()`.
  // In order to obtain correct results, we need to compare the results before
  // and after `openTabAndSetupStorage` is called.
  // To ensure more accurate results, try choosing a uncommon origin for PREFIX.
  const EXISTING_ORIGINS = await listOrigins();
  expectedOrigins.forEach(expected => {
    ok(!EXISTING_ORIGINS.includes(expected), `${expected} doesn't exist`);
  });

  await openTabAndSetupStorage(TEST_URL);

  const origins = await listOrigins();
  for (const origin of origins) {
    ok(
      EXISTING_ORIGINS.includes(origin) || expectedOrigins.includes(origin),
      `check origin: ${origin}`
    );
  }
  expectedOrigins.forEach(expected => {
    ok(origins.includes(expected), `${expected} is added`);
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
