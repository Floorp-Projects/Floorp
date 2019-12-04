/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource:///modules/SitePermissions.jsm", this);

// This asserts that SitePermissions.set can not save ALLOW permissions
// temporarily on a tab.
add_task(async function testTempAllowThrows() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );
  let id = "notifications";

  await BrowserTestUtils.withNewTab(principal.URI.spec, function(browser) {
    Assert.throws(function() {
      SitePermissions.setForPrincipal(
        principal,
        id,
        SitePermissions.ALLOW,
        SitePermissions.SCOPE_TEMPORARY,
        browser
      );
    }, /'Block' is the only permission we can save temporarily on a browser/);
  });
});

// This tests the SitePermissions.getAllPermissionDetailsForBrowser function.
add_task(async function testGetAllPermissionDetailsForBrowser() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    principal.URI.spec
  );

  Services.prefs.setIntPref("permissions.default.shortcuts", 2);

  SitePermissions.setForPrincipal(principal, "camera", SitePermissions.ALLOW);
  SitePermissions.setForPrincipal(
    principal,
    "cookie",
    SitePermissions.ALLOW_COOKIES_FOR_SESSION
  );
  SitePermissions.setForPrincipal(principal, "popup", SitePermissions.BLOCK);
  SitePermissions.setForPrincipal(
    principal,
    "geo",
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_SESSION
  );
  SitePermissions.setForPrincipal(
    principal,
    "shortcuts",
    SitePermissions.ALLOW
  );

  let permissions = SitePermissions.getAllPermissionDetailsForBrowser(
    tab.linkedBrowser
  );

  let camera = permissions.find(({ id }) => id === "camera");
  Assert.deepEqual(camera, {
    id: "camera",
    label: "Use the Camera",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that removed permissions (State.UNKNOWN) are skipped.
  SitePermissions.removeFromPrincipal(principal, "camera");
  permissions = SitePermissions.getAllPermissionDetailsForBrowser(
    tab.linkedBrowser
  );

  camera = permissions.find(({ id }) => id === "camera");
  Assert.equal(camera, undefined);

  let cookie = permissions.find(({ id }) => id === "cookie");
  Assert.deepEqual(cookie, {
    id: "cookie",
    label: "Set Cookies",
    state: SitePermissions.ALLOW_COOKIES_FOR_SESSION,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  let popup = permissions.find(({ id }) => id === "popup");
  Assert.deepEqual(popup, {
    id: "popup",
    label: "Open Pop-up Windows",
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  let geo = permissions.find(({ id }) => id === "geo");
  Assert.deepEqual(geo, {
    id: "geo",
    label: "Access Your Location",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_SESSION,
  });

  let shortcuts = permissions.find(({ id }) => id === "shortcuts");
  Assert.deepEqual(shortcuts, {
    id: "shortcuts",
    label: "Override Keyboard Shortcuts",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  SitePermissions.removeFromPrincipal(principal, "cookie");
  SitePermissions.removeFromPrincipal(principal, "popup");
  SitePermissions.removeFromPrincipal(principal, "geo");
  SitePermissions.removeFromPrincipal(principal, "shortcuts");

  Services.prefs.clearUserPref("permissions.default.shortcuts");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
