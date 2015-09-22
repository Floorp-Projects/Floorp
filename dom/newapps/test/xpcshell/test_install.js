/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/PermissionSettings.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

const mod = Cc['@mozilla.org/newapps/installpackagedwebapp;1']
                  .getService(Ci.nsIInstallPackagedWebapp);

XPCOMUtils.defineLazyServiceGetter(this,
                                   "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

function run_test() {

  let manifestWithPerms = {
    name: "Test App",
    launch_path: "/index.html",
    type: "privileged",
    permissions: {
      "alarms": { },
      "wifi-manage": { },
      "tcp-socket": { },
      "desktop-notification": { },
      "geolocation": { },
    },
  };

  let manifestNoPerms = {
    name: "Test App",
    launch_path: "/index.html",
    type: "privileged",
  };

  let appStatus = "privileged";

  // Triggering error due to bad manifest
  let origin = "";
  let manifestURL = "";
  let manifestString = "boum";

  let res = mod.installPackagedWebapp(manifestString, origin, manifestURL);
  equal(res, false);

  // Install a package with permissions
  origin = "http://test.com^appId=1019&inBrowser=1";
  manifestURL = "http://test.com/manifest.json";
  manifestString = JSON.stringify(manifestWithPerms);
  let manifestHelper = new ManifestHelper(manifestWithPerms, origin, manifestURL);

  cleanDB(manifestHelper, origin, manifestURL);

  res = mod.installPackagedWebapp(manifestString, origin, manifestURL);
  equal(res, true);
  checkPermissions(manifestHelper, origin, manifestURL, appStatus);

  // Install a package with permissions
  origin = "http://test.com";
  manifestHelper = new ManifestHelper(manifestWithPerms, origin, manifestURL);

  cleanDB(manifestHelper, origin, manifestURL);

  res = mod.installPackagedWebapp(manifestString, origin, manifestURL);
  equal(res, true);
  checkPermissions(manifestHelper, origin, manifestURL, appStatus);


  // Install a package with no permission
  origin = "http://bar.com^appId=1337&inBrowser=1";
  manifestURL = "http://bar.com/manifest.json";
  manifestString = JSON.stringify(manifestNoPerms);
  manifestHelper = new ManifestHelper(manifestNoPerms, origin, manifestURL);

  cleanDB(manifestHelper, origin, manifestURL);

  res = mod.installPackagedWebapp(manifestString, origin, manifestURL);
  equal(res, true);
  checkPermissions(manifestHelper, origin, manifestURL, appStatus);
}

// Cleaning permissions database before running a test
function cleanDB(manifestHelper, origin, manifestURL) {
  for (let permName in manifestHelper.permissions) {
    PermissionSettingsModule.removePermission(permName, manifestURL, origin, "", true);
  }
}

// Check permissions are correctly set in the database
function checkPermissions(manifestHelper, origin, manifestURL, appStatus) {
  let perm;
  for (let permName in manifestHelper.permissions) {
    let permValue = PermissionSettingsModule.getPermission(
        permName, manifestURL, origin, "", true);
    switch (PermissionsTable[permName][appStatus]) {
      case Ci.nsIPermissionManager.UNKNOWN_ACTION:
        perm = "unknown";
        break;
      case Ci.nsIPermissionManager.ALLOW_ACTION:
        perm = "allow";
        break;
      case Ci.nsIPermissionManager.DENY_ACTION:
        perm = "deny";
        break;
      case Ci.nsIPermissionManager.PROMPT_ACTION:
        perm = "prompt";
        break;
      default:
        break;
    }
    equal(permValue, perm);
  }
}
