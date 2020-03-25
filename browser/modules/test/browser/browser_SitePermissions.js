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

  SitePermissions.setForPrincipal(principal, "xr", SitePermissions.ALLOW);

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

  let xr = permissions.find(({ id }) => id === "xr");
  Assert.deepEqual(xr, {
    id: "xr",
    label: "Access Virtual Reality Devices",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  SitePermissions.removeFromPrincipal(principal, "cookie");
  SitePermissions.removeFromPrincipal(principal, "popup");
  SitePermissions.removeFromPrincipal(principal, "geo");
  SitePermissions.removeFromPrincipal(principal, "shortcuts");

  SitePermissions.removeFromPrincipal(principal, "xr");

  Services.prefs.clearUserPref("permissions.default.shortcuts");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testInvalidPrincipal() {
  // Check that an error is thrown when an invalid principal argument is passed.
  try {
    SitePermissions.isSupportedPrincipal("file:///example.js");
  } catch (e) {
    Assert.equal(
      e.message,
      "Argument passed as principal is not an instance of Ci.nsIPrincipal"
    );
  }
  try {
    SitePermissions.removeFromPrincipal(null, "canvas");
  } catch (e) {
    Assert.equal(
      e.message,
      "Atleast one of the arguments, either principal or browser should not be null."
    );
  }
  try {
    SitePermissions.setForPrincipal(
      "blah",
      "camera",
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_PERSISTENT,
      gBrowser.selectedBrowser
    );
  } catch (e) {
    Assert.equal(
      e.message,
      "Argument passed as principal is not an instance of Ci.nsIPrincipal"
    );
  }
  try {
    SitePermissions.getAllByPrincipal("blah");
  } catch (e) {
    Assert.equal(
      e.message,
      "Argument passed as principal is not an instance of Ci.nsIPrincipal"
    );
  }
  try {
    SitePermissions.getAllByPrincipal(null);
  } catch (e) {
    Assert.equal(e.message, "principal argument cannot be null.");
  }
  try {
    SitePermissions.getForPrincipal(5, "camera");
  } catch (e) {
    Assert.equal(
      e.message,
      "Argument passed as principal is not an instance of Ci.nsIPrincipal"
    );
  }
  // Check that no error is thrown when passing valid principal and browser arguments.
  Assert.deepEqual(
    SitePermissions.getForPrincipal(gBrowser.contentPrincipal, "camera"),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    }
  );
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, "camera", gBrowser.selectedBrowser),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    }
  );
});
