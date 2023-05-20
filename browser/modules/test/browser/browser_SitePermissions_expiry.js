/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const EXPIRE_TIME_MS = 100;
const TIMEOUT_MS = 500;

// This tests the time delay to expire temporary permission entries.
add_task(async function testTemporaryPermissionExpiry() {
  SpecialPowers.pushPrefEnv({
    set: [["privacy.temporary_permission_expire_time_ms", EXPIRE_TIME_MS]],
  });

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.com"
    );
  let id = "camera";

  await BrowserTestUtils.withNewTab(principal.spec, async function (browser) {
    SitePermissions.setForPrincipal(
      principal,
      id,
      SitePermissions.BLOCK,
      SitePermissions.SCOPE_TEMPORARY,
      browser
    );

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    await new Promise(c => setTimeout(c, TIMEOUT_MS));

    Assert.deepEqual(SitePermissions.getForPrincipal(principal, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });
  });
});
