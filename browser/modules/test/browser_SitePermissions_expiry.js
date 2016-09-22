/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);

// This tests the time delay to expire temporary permission entries.
add_task(function* testTemporaryPermissionExpiry() {
  SpecialPowers.pushPrefEnv({set: [
        ["privacy.temporary_permission_expire_time_ms", 100],
  ]});

  let uri = Services.io.newURI("https://example.com")
  let id = "camera";

  yield BrowserTestUtils.withNewTab(uri.spec, function*(browser) {
    SitePermissions.set(uri, id, SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY, browser);

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    });

    yield new Promise((c) => setTimeout(c, 500));

    Assert.deepEqual(SitePermissions.get(uri, id, browser), {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    });
  });
});
