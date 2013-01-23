/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/PermissionSettings.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");

this.EXPORTED_SYMBOLS = ["PermissionsInstaller"];
const UNKNOWN_ACTION = Ci.nsIPermissionManager.UNKNOWN_ACTION;
const ALLOW_ACTION = Ci.nsIPermissionManager.ALLOW_ACTION;
const DENY_ACTION = Ci.nsIPermissionManager.DENY_ACTION;
const PROMPT_ACTION = Ci.nsIPermissionManager.PROMPT_ACTION;

// Permission access flags
const READONLY = "readonly";
const CREATEONLY = "createonly";
const READCREATE = "readcreate";
const READWRITE = "readwrite";

const PERM_TO_STRING = ["unknown", "allow", "deny", "prompt"];

function debug(aMsg) {
  //dump("-*-*- PermissionsInstaller.jsm : " + aMsg + "\n");
}

// An array carring all the possible (expanded) permission names.
let AllPossiblePermissions = [];
for (let permName in PermissionsTable) {
  let expandedPermNames = [];
  if (PermissionsTable[permName].access) {
    expandedPermNames = expandPermissions(permName, READWRITE);
  } else {
    expandedPermNames = expandPermissions(permName);
  }
  AllPossiblePermissions = AllPossiblePermissions.concat(expandedPermNames);
}

this.PermissionsInstaller = {
  /**
   * Install permissisions or remove deprecated permissions upon re-install.
   * @param object aApp
   *        The just-installed app configuration.
   *        The properties used are manifestURL, origin and manifest.
   * @param boolean aIsReinstall
   *        Indicates the app was just re-installed
   * @param function aOnError
   *        A function called if an error occurs
   * @returns void
   **/
  installPermissions: function installPermissions(aApp, aIsReinstall, aOnError) {
    try {
      let newManifest = new ManifestHelper(aApp.manifest, aApp.origin);
      if (!newManifest.permissions && !aIsReinstall) {
        return;
      }

      if (aIsReinstall) {
        // Compare the original permissions against the new permissions
        // Remove any deprecated Permissions

        if (newManifest.permissions) {
          // Expand permission names.
          let newPermNames = [];
          for (let permName in newManifest.permissions) {
            let expandedPermNames =
              expandPermissions(permName,
                                newManifest.permissions[permName].access);
            newPermNames = newPermNames.concat(expandedPermNames);
          }

          for (let idx in AllPossiblePermissions) {
            let permName = AllPossiblePermissions[idx];
            let index = newPermNames.indexOf(permName);
            if (index == -1) {
              // See if the permission was installed previously.
              let permValue =
                PermissionSettingsModule.getPermission(permName,
                                                       aApp.manifestURL,
                                                       aApp.origin,
                                                       false);
              if (permValue == "unknown" || permValue == "deny") {
                // All 'deny' permissions should be preserved
                continue;
              }
              // Remove the deprecated permission
              // TODO: use PermSettings.remove, see bug 793204
              this._setPermission(permName, "unknown", aApp);
            }
          }
        }
      }

      // Check to see if the 'webapp' is app/privileged/certified.
      let appStatus;
      switch (AppsUtils.getAppManifestStatus(aApp.manifest)) {
      case Ci.nsIPrincipal.APP_STATUS_CERTIFIED:
        appStatus = "certified";
        break;
      case Ci.nsIPrincipal.APP_STATUS_PRIVILEGED:
        appStatus = "privileged";
        break;
      case Ci.nsIPrincipal.APP_STATUS_INSTALLED:
        appStatus = "app";
        break;
      default:
        // Cannot determine app type, abort install by throwing an error.
        throw new Error("PermissionsInstaller.jsm: " +
                        "Cannot determine the app's status. Install cancelled.");
        break;
      }

      for (let permName in newManifest.permissions) {
        if (!PermissionsTable[permName]) {
          Cu.reportError("PermissionsInstaller.jsm: '" + permName + "'" +
                         " is not a valid Webapps permission name.");
          dump("PermissionsInstaller.jsm: '" + permName + "'" +
               " is not a valid Webapps permission name.");
          continue;
        }

        let expandedPermNames =
          expandPermissions(permName,
                            newManifest.permissions[permName].access);
        for (let idx in expandedPermNames) {
          this._setPermission(expandedPermNames[idx],
                              PERM_TO_STRING[PermissionsTable[permName][appStatus]],
                              aApp);
        }
      }
    }
    catch (ex) {
      dump("Caught webapps install permissions error for " + aApp.origin);
      Cu.reportError(ex);
      if (aOnError) {
        aOnError();
      }
    }
  },

  /**
   * Set a permission value.
   * @param string aPermName
   *        The permission name.
   * @param string aPermValue
   *        The permission value.
   * @param object aApp
   *        The just-installed app configuration.
   *        The properties used are manifestURL and origin.
   * @returns void
   **/
  _setPermission: function setPermission(aPermName, aPermValue, aApp) {
    PermissionSettingsModule.addPermission({
      type: aPermName,
      origin: aApp.origin,
      manifestURL: aApp.manifestURL,
      value: aPermValue,
      browserFlag: false
    });
  }
};
