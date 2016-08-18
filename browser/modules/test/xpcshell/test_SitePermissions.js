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
  let wrongURI = Services.io.newURI("file:///example.js", null, null)
  Assert.deepEqual(SitePermissions.getAllByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com", null, null)
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW }
  ]);

  SitePermissions.set(uri, "microphone", SitePermissions.SESSION);
  SitePermissions.set(uri, "desktop-notification", SitePermissions.BLOCK);

  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW },
      { id: "microphone", state: SitePermissions.SESSION },
      { id: "desktop-notification", state: SitePermissions.BLOCK }
  ]);

  SitePermissions.remove(uri, "microphone");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), [
      { id: "camera", state: SitePermissions.ALLOW },
      { id: "desktop-notification", state: SitePermissions.BLOCK }
  ]);

  SitePermissions.remove(uri, "camera");
  SitePermissions.remove(uri, "desktop-notification");
  Assert.deepEqual(SitePermissions.getAllByURI(uri), []);
});

add_task(function* testGetPermissionDetailsByURI() {
  // check that it returns an empty array on an invalid URI
  // like a file URI, which doesn't support site permissions
  let wrongURI = Services.io.newURI("file:///example.js", null, null)
  Assert.deepEqual(SitePermissions.getPermissionDetailsByURI(wrongURI), []);

  let uri = Services.io.newURI("https://example.com", null, null)

  SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
  SitePermissions.set(uri, "cookie", SitePermissions.SESSION);
  SitePermissions.set(uri, "popup", SitePermissions.BLOCK);

  let permissions = SitePermissions.getPermissionDetailsByURI(uri);

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
  permissions = SitePermissions.getPermissionDetailsByURI(uri);

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
