/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

var EXPORTED_SYMBOLS = ["PermissionsInstaller"];

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

XPCOMUtils.defineLazyServiceGetter(this,
                                   "PermSettings",
                                   "@mozilla.org/permissionSettings;1",
                                   "nsIDOMPermissionSettings");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "permissionManager",
                                   "@mozilla.org/permissionmanager;1",
                                   "nsIPermissionManager");

function debug(aMsg) {
  //dump("-*-*- PermissionsInstaller.jsm : " + aMsg + "\n");
}

/**
 * Converts ['read', 'write'] to ['contacts-read', 'contacts-write'], etc...
 * @param string aPermName
 * @param Array aSuffixes
 * @returns Array
 **/
function mapSuffixes(aPermName, aSuffixes)
{
  return aSuffixes.map(function(suf) { return aPermName + "-" + suf; });
}

// Permissions Matrix: https://docs.google.com/spreadsheet/ccc?key=0Akyz_Bqjgf5pdENVekxYRjBTX0dCXzItMnRyUU1RQ0E#gid=0

// Permissions that are implicit:
// battery-status, network-information, vibration,
// device-capabilities

const PermissionsTable = { "resource-lock": {
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           geolocation: {
                             app: PROMPT_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION
                           },
                           camera: {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION
                           },
                           alarms: {
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "tcp-socket": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "network-events": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           contacts: {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "device-storage:apps": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "device-storage:pictures": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "device-storage:videos": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "device-storage:music": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           sms: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           telephony: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           browser: {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           bluetooth: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           wifi: {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION
                           },
                           keyboard: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           mobileconnection: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           power: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           push: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           settings: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           permissions: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           fmradio: {
                             app: ALLOW_ACTION,     // Matrix indicates '?'
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           attention: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "webapps-manage": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "backgroundservice": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "desktop-notification": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "networkstats-manage": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "mozBluetooth": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "wifi-manage": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "systemXHR": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "voicemail": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "deprecated-hwvideo": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "idle": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "time": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "embed-apps": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "storage": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                         };

// Sometimes all permissions (fully expanded) need to be iterated through
let AllPossiblePermissions = [];
for (let permName in PermissionsTable) {
  if (PermissionsTable[permName].access) {
    AllPossiblePermissions =
      AllPossiblePermissions.concat(mapSuffixes(permName,
                                    PermissionsTable[permName].access));
  }
  else {
    AllPossiblePermissions.push(permName);
  }
}

/**
 * Expand an access string into multiple permission names,
 *   e.g: perm 'contacts' with 'readwrite' =
 *   ['contacts-read', 'contacts-create', contacts-write']
 * @param string aPermName
 * @param string aAccess
 * @returns Array
 **/
function expandPermissions(aPermName, aAccess) {
  if (!PermissionsTable[aPermName]) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Unknown Permission: " + aPermName);
    throw new Error("PermissionsTable.jsm: expandPermissions: Unknown Permission: " + aPermName);
  }
  if (!aAccess && PermissionsTable[aPermName].access ||
      aAccess && !PermissionsTable[aPermName].access) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Invalid Manifest : " +
                   aPermName + " " + aAccess + "\n");
    throw new Error("PermissionsTable.jsm: expandPermissions: Invalid Manifest");
  }
  if (!PermissionsTable[aPermName].access) {
    return [aPermName];
  }

  let requestedSuffixes = [];
  switch(aAccess) {
  case READONLY:
    requestedSuffixes.push("read");
    break;
  case CREATEONLY:
    requestedSuffixes.push("create");
    break;
  case READCREATE:
    requestedSuffixes.push("read", "create");
    break;
  case READWRITE:
    requestedSuffixes.push("read", "create", "write");
    break;
  default:
    return [];
  }

  let permArr = mapSuffixes(aPermName, requestedSuffixes);

  let expandedPerms = [];
  for (let idx in permArr) {
    if (PermissionsTable[aPermName].access.indexOf(requestedSuffixes[idx]) != -1) {
      expandedPerms.push(permArr[idx]);
    }
  }
  return expandedPerms;
}

let PermissionsInstaller = {
/**
   * Install permissisions or remove deprecated permissions upon re-install
   * @param object aApp
   *        The just-installed app configuration.
            The properties used are manifestURL, origin and manifest.
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
          // Expand perms
          let newPerms = [];
          for (let perm in newManifest.permissions) {
            let _perms = expandPermissions(perm,
                                           newManifest.permissions[perm].access);
            newPerms = newPerms.concat(_perms);
          }

          for (let idx in AllPossiblePermissions) {
            let index = newPerms.indexOf(AllPossiblePermissions[idx]);
            if (index == -1) {
              // See if the permission was installed previously
              let _perm = PermSettings.get(AllPossiblePermissions[idx],
                                           aApp.manifestURL,
                                           aApp.origin,
                                           false);
              if (_perm == "unknown" || _perm == "deny") {
                // All 'deny' permissions should be preserved
                continue;
              }
              // Remove the deprecated permission
              // TODO: use PermSettings.remove, see bug 793204
              this._setPermission(AllPossiblePermissions[idx], "unknown", aApp);
            }
          }
        }
      }

      let installPermType;
      // Check to see if the 'webapp' is app/priv/certified
      switch (AppsUtils.getAppManifestStatus(aApp.manifest)) {
      case Ci.nsIPrincipal.APP_STATUS_CERTIFIED:
        installPermType = "certified";
        break;
      case Ci.nsIPrincipal.APP_STATUS_PRIVILEGED:
        installPermType = "privileged";
        break;
      case Ci.nsIPrincipal.APP_STATUS_INSTALLED:
        installPermType = "app";
        break;
      default:
        // Cannot determine app type, abort install by throwing an error
        throw new Error("PermissionsInstaller.jsm: Cannot determine app type, install cancelled");
      }

      for (let permName in newManifest.permissions) {
        if (!PermissionsTable[permName]) {
          throw new Error("PermissionsInstaller.jsm: '" + permName + "'" +
                         " is not a valid Webapps permission type. Aborting Webapp installation");
          return;
        }

        let perms = expandPermissions(permName,
                                      newManifest.permissions[permName].access);
        for (let idx in perms) {
          let perm = PermissionsTable[permName][installPermType];
          let permValue = PERM_TO_STRING[perm];
          this._setPermission(perms[idx], permValue, aApp);
        }
      }
    }
    catch (ex) {
      debug("Caught webapps install permissions error");
      Cu.reportError(ex);
      if (aOnError) {
        aOnError();
      }
    }
  },

  /**
   * Set a permission value, replacing "storage" if needed.
   * @param string aPerm
   *        The permission name.
   * @param string aValue
   *        The permission value.
   * @param object aApp
   *        The just-installed app configuration.
            The properties used are manifestURL, origin and manifest.
   * @returns void
   **/
  _setPermission: function setPermission(aPerm, aValue, aApp) {
    dump("XxXxX setting " + aPerm + " to " + aValue + "\n");
    if (aPerm != "storage") {
      PermSettings.set(aPerm, aValue, aApp.manifestURL, aApp.origin, false);
      return;
    }

    ["indexedDB-unlimited", "offline-app", "pin-app"].forEach(
      function(aName) {
        PermSettings.set(aName, aValue, aApp.manifestURL, aApp.origin, false);
      }
    );
  }
}
