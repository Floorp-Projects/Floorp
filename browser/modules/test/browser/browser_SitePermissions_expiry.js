/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);

const EXPIRE_TIME_MS = 100;
const TIMEOUT_MS = 500;

// This tests the time delay to expire temporary permission entries.
add_task(async function testTemporaryPermissionExpiry() {
  SpecialPowers.pushPrefEnv({set: [
        ["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS],
  ]});

  let uri = Services.io.newURI("https://example.com");
  let id = "camera";

  await BrowserTestUtils.withNewTab(uri.spec, async function(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    await new Promise((c) => setTimeout(c, TIMEOUT_MS));

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });
  });
});
