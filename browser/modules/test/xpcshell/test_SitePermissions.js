/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/SitePermissions.jsm");

add_task(function* testPermissionsListing() {
  Assert.deepEqual(SitePermissions.listPermissions().sort(),
    ["camera","cookie","desktop-notification","geo","image",
     "indexedDB","install","microphone","pointerLock","popup",
     "push"],
    "Correct list of all permissions");

  Assert.deepEqual(SitePermissions.listPageFunctionalityPermissions().sort(),
    ["cookie","desktop-notification","image","install","popup"],
    "Correct list of 'page functionality' permissions");

  Assert.deepEqual(SitePermissions.listSystemAccessPermissions().sort(),
    ["camera","geo","indexedDB","microphone","pointerLock","push"],
    "Correct list of 'page functionality' permissions");
});
