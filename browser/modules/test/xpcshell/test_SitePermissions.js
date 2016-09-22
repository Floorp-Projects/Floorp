/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

add_task(function* testPermissionsListing() {
  Assert.deepEqual(SitePermissions.listPermissions().sort(),
    ["camera", "cookie", "desktop-notification", "geo", "image",
     "indexedDB", "install", "microphone", "popup", "screen"],
    "Correct list of all permissions");
});

add_task(function* testGetAllByURI() {
  // check that it returns an empty array on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js")
  Assert.deepEqual(SitePermissions.getAllByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com")
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.set(uri, "microphone", SitePermissions.ALLOW, SitePermissions.SCOPE_SESSION);
  SitePermissions.set(uri, "desktop-notification", SitePermissions.BLOCK);

  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT },
      { id: "microphone", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_SESSION },
      { id: "desktop-notification", state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.remove(uri, "microphone");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW, scope: SitePermissions.SCOPE_PERSISTENT },
      { id: "desktop-notification", state: SitePermissions.BLOCK, scope: SitePermissions.SCOPE_PERSISTENT }
  ]);

  SitePermissions.remove(uri, "camera");
  SitePermissions.remove(uri, "desktop-notification");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);

  // XXX Bug 1303108 - Control Center should only show non-default permissions
  SitePermissions.set(uri, "addon", SitePermissions.BLOCK);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);
  SitePermissions.remove(uri, "addon");
});
