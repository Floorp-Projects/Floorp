/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/PermissionSettings.jsm");

this.EXPORTED_SYMBOLS = ["PermissionsInstaller",
                         "expandPermissions",
                         "PermissionsTable",
                        ];
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
// Also, keep in sync with https://mxr.mozilla.org/mozilla-central/source/extensions/cookie/Permission.txt

// Permissions that are implicit:
// battery-status, network-information, vibration,
// device-capabilities

this.PermissionsTable =  { "resource-lock": {
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
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
                           },
                           "device-storage:apps": {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
                           },
                           "device-storage:pictures": {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
                           },
                           "device-storage:videos": {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
                           },
                           "device-storage:music": {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
                           },
                           "device-storage:sdcard": {
                             app: DENY_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read", "write", "create"]
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
                             certified: ALLOW_ACTION,
                             access: ["read", "write"],
                             additional: ["indexedDB-chrome-settings"]
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
                             privileged: ALLOW_ACTION,
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
                             certified: ALLOW_ACTION,
                             substitute: [
                               "indexedDB-unlimited",
                               "offline-app",
                               "pin-app"
                             ]
                           },
                           "background-sensors": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                         };

/**
 * Expand an access string into multiple permission names,
 *   e.g: perm 'contacts' with 'readwrite' =
 *   ['contacts-read', 'contacts-create', contacts-write']
 * @param string aPermName
 * @param string aAccess
 * @returns Array
 **/
this.expandPermissions = function expandPermissions(aPermName, aAccess) {
  if (!PermissionsTable[aPermName]) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Unknown Permission: " + aPermName);
    dump("PermissionsTable.jsm: expandPermissions: Unknown Permission: " + aPermName);
    return [];
  }

  const tableEntry = PermissionsTable[aPermName];

  if (tableEntry.substitute && tableEntry.additional) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Can't handle both 'substitute' " +
                   "and 'additional' entries for permission: " + aPermName);
    return [];
  }

  if (!aAccess && tableEntry.access ||
      aAccess && !tableEntry.access) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Invalid Manifest : " +
                   aPermName + " " + aAccess + "\n");
    dump("PermissionsTable.jsm: expandPermissions: Invalid Manifest: " +
         aPermName + " " + aAccess + "\n");
    throw new Error("PermissionsTable.jsm: expandPermissions: Invalid Manifest: " +
                    aPermName + " " + aAccess + "\n");
  }

  let expandedPerms = [];

  if (tableEntry.access && aAccess) {
  let requestedSuffixes = [];
    switch (aAccess) {
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

  // Add the same suffix to each of the additions.
  if (tableEntry.additional) {
    for each (let additional in tableEntry.additional) {
      permArr = permArr.concat(mapSuffixes(additional, requestedSuffixes));
    }
  }

  // Only add the suffixed version if the suffix exisits in the table.
  for (let idx in permArr) {
      let suffix = requestedSuffixes[idx % requestedSuffixes.length];
      if (tableEntry.access.indexOf(suffix) != -1) {
      expandedPerms.push(permArr[idx]);
    }
  }
  } else if (tableEntry.substitute) {
    expandedPerms = expandedPerms.concat(tableEntry.substitute);
  } else {
    expandedPerms.push(aPermName);
    // Include each of the additions exactly as they appear in the table.
    if (tableEntry.additional) {
      expandedPerms = expandedPerms.concat(tableEntry.additional);
    }
  }

  return expandedPerms;
};

// Sometimes all permissions (fully expanded) need to be iterated through
let AllPossiblePermissions = [];
for (let permName in PermissionsTable) {
  if (PermissionsTable[permName].access) {
    AllPossiblePermissions =
      AllPossiblePermissions.concat(expandPermissions(permName, READWRITE));
  } else {
    AllPossiblePermissions =
      AllPossiblePermissions.concat(expandPermissions(permName));
  }
}

this.PermissionsInstaller = {
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
              let _perm = PermissionSettingsModule.getPermission(AllPossiblePermissions[idx],
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
          Cu.reportError("PermissionsInstaller.jsm: '" + permName + "'" +
                         " is not a valid Webapps permission type.");
          dump("PermissionsInstaller.jsm: '" + permName + "'" +
               " is not a valid Webapps permission type.");
          continue;
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
      dump("Caught webapps install permissions error for " + aApp.origin);
      Cu.reportError(ex);
      if (aOnError) {
        aOnError();
      }
    }
  },

  /**
   * Set a permission value
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
      PermissionSettingsModule.addPermission({
        type: aPerm,
        origin: aApp.origin,
        manifestURL: aApp.manifestURL,
        value: aValue,
        browserFlag: false
      });
    }
};
