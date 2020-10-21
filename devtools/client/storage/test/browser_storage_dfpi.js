/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

// Basic test to assert that the storage tree and table corresponding to each
// item in the storage tree is correctly displayed

"use strict";

// Ensure iframe.src in storage-dfpi.html starts with PREFIX.
const PREFIX = "http://sub1.test1.example";
const ORIGIN = `${PREFIX}.org`;
const TEST_URL = `${ORIGIN}/${PATH}storage-dfpi.html`;

function listOrigins() {
  return new Promise(resolve => {
    SpecialPowers.Services.qms.listOrigins().callback = req => {
      resolve(req.result);
    };
  });
}

async function clearAllDatabases() {
  await new Promise(resolve => {
    const uri = SpecialPowers.Services.io.newURI(TEST_URL);
    const attrs = {};
    const principal = SpecialPowers.Services.scriptSecurityManager.createContentPrincipal(
      uri,
      attrs
    );
    const request = SpecialPowers.Services.qms.clearStoragesForPrincipal(
      principal
    );
    const cb = SpecialPowers.wrapCallback(resolve);
    request.callback = cb;
  });
}

add_task(async function() {
  await pushPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );

  registerCleanupFunction(clearAllDatabases);

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
});
