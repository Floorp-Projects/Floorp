/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/SitePermissions.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

add_task(function* testPermissionsListing() {
  Assert.deepEqual(SitePermissions.listPermissions().sort(),
    ["camera","cookie","desktop-notification","geo","image",
     "indexedDB","install","microphone","pointerLock","popup"],
    "Correct list of all permissions");
});

add_task(function* testHasGrantedPermissions() {
  let uri = Services.io.newURI("https://example.com", null, null)
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), false);

  // check that ALLOW states return true
  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), true);

  // removing the ALLOW state should revert to false
  SitePermissions.remove(uri, "camera");
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), false);

  // check that SESSION states return true
  SitePermissions.set(uri, "microphone", SitePermissions.SESSION);
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), true);

  // removing the SESSION state should revert to false
  SitePermissions.remove(uri, "microphone");
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), false);

  SitePermissions.set(uri, "pointerLock", SitePermissions.BLOCK);

  // check that a combination of ALLOW and BLOCK states returns true
  SitePermissions.set(uri, "geo", SitePermissions.ALLOW);
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), true);

  // check that a combination of SESSION and BLOCK states returns true
  SitePermissions.set(uri, "geo", SitePermissions.SESSION);
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), true);

  // check that only BLOCK states will not return true
  SitePermissions.remove(uri, "geo");
  Assert.equal(SitePermissions.hasGrantedPermissions(uri), false);

  SitePermissions.remove(uri, "pointerLock");
});
