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
  // check that it returns false on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js", null, null)
  Assert.equal(SitePermissions.hasGrantedPermissions(wrongURI), false);

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

add_task(function* testGetPermissionsByURI() {
  // check that it returns an empty array on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js", null, null)
  Assert.deepEqual(SitePermissions.getPermissionsByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com", null, null)

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  SitePermissions.set(uri, "cookie", SitePermissions.SESSION);
  SitePermissions.set(uri, "popup", SitePermissions.BLOCK);

  let permissions = SitePermissions.getPermissionsByURI(uri);

  let camera = permissions.find(({id}) => id === "camera");
  Assert.deepEqual(camera, {
    id: "camera",
    label: "Use the Camera",
    state: SitePermissions.ALLOW,
    availableStates: [
      { id: SitePermissions.UNKNOWN, label: "Always Ask" },
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  // check that removed permissions (State.UNKNOWN) are skipped
  SitePermissions.remove(uri, "camera");
  permissions = SitePermissions.getPermissionsByURI(uri);

  camera = permissions.find(({id}) => id === "camera");
  Assert.equal(camera, undefined);

  // check that different available state values are represented

  let cookie = permissions.find(({id}) => id === "cookie");
  Assert.deepEqual(cookie, {
    id: "cookie",
    label: "Set Cookies",
    state: SitePermissions.SESSION,
    availableStates: [
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.SESSION, label: "Allow for Session" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  let popup = permissions.find(({id}) => id === "popup");
  Assert.deepEqual(popup, {
    id: "popup",
    label: "Open Pop-up Windows",
    state: SitePermissions.BLOCK,
    availableStates: [
      { id: SitePermissions.ALLOW, label: "Allow" },
      { id: SitePermissions.BLOCK, label: "Block" },
    ]
  });

  SitePermissions.remove(uri, "cookie");
  SitePermissions.remove(uri, "popup");
});
