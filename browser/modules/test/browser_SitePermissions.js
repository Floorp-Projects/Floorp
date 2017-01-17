/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);

// This asserts that SitePermissions.set can not save ALLOW permissions
// temporarily on a tab.
add_task(function* testTempAllowThrows() {
  let uri = Services.io.newURI("https://example.com");
  let id = "notifications";

  yield BrowserTestUtils.withNewTab(uri.spec, function(browser) {
    Assert.throws(function() {
      SitePermissions.set(uri, id, SitePermissions.ALLOW, SitePermissions.SCOPE_TEMPORARY, browser);
    }, "'Block' is the only permission we can save temporarily on a tab");
  });
});

// This tests the SitePermissions.getAllPermissionDetailsForBrowser function.
add_task(function* testGetAllPermissionDetailsForBrowser() {
  let uri = Services.io.newURI("https://example.com");

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, uri.spec);

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  SitePermissions.set(uri, "cookie", SitePermissions.ALLOW_COOKIES_FOR_SESSION);
  SitePermissions.set(uri, "popup", SitePermissions.BLOCK);
  SitePermissions.set(uri, "geo", SitePermissions.ALLOW, SitePermissions.SCOPE_SESSION);

  let permissions = SitePermissions.getAllPermissionDetailsForBrowser(tab.linkedBrowser);

  let camera = permissions.find(({id}) => id === "camera");
  Assert.deepEqual(camera, {
    id: "camera",
    label: "Use the Camera",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
    availableStates: [
      { id: SitePermissions.UNKNOWN, label: "Always Ask" },
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  // check that removed permissions (State.UNKNOWN) are skipped
  SitePermissions.remove(uri, "camera");
  permissions = SitePermissions.getAllPermissionDetailsForBrowser(tab.linkedBrowser);

  camera = permissions.find(({id}) => id === "camera");
  Assert.equal(camera, undefined);

  // check that different available state values are represented

  let cookie = permissions.find(({id}) => id === "cookie");
  Assert.deepEqual(cookie, {
    id: "cookie",
    label: "Set Cookies",
    state: SitePermissions.ALLOW_COOKIES_FOR_SESSION,
    scope: SitePermissions.SCOPE_PERSISTENT,
    availableStates: [
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.ALLOW_COOKIES_FOR_SESSION, label: "Allow for Session" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  let popup = permissions.find(({id}) => id === "popup");
  Assert.deepEqual(popup, {
    id: "popup",
    label: "Open Pop-up Windows",
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
    availableStates: [
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  let geo = permissions.find(({id}) => id === "geo");
  Assert.deepEqual(geo, {
    id: "geo",
    label: "Access Your Location",
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_SESSION,
    availableStates: [
      { id: SitePermissions.UNKNOWN, label: "Always Ask" },
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  SitePermissions.remove(uri, "cookie");
  SitePermissions.remove(uri, "popup");
  SitePermissions.remove(uri, "geo");

  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

