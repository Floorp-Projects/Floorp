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
                         "appendAccessToPermName"
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

// Permissions Matrix: https://docs.google.com/spreadsheet/ccc?key=0Akyz_Bqjgf5pdENVekxYRjBTX0dCXzItMnRyUU1RQ0E#gid=0
// Also, keep in sync with https://mxr.mozilla.org/mozilla-central/source/extensions/cookie/Permission.txt

// Permissions that are implicit:
// battery-status, network-information, vibration,
// device-capabilities

this.PermissionsTable =  { geolocation: {
                             app: PROMPT_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: ALLOW_ACTION
                           },
                           camera: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
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
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION,
                             access: ["read"]
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
                             app: ALLOW_ACTION,
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
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "networkstats-manage": {
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
                           cellbroadcast: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-normal": {
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-content": {
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-notification": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-alarm": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-telephony": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-ringer": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-channel-publicnotification": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "open-remote-window": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                         };

/**
 * Append access modes to the permission name as suffixes.
 *   e.g. permission name 'contacts' with ['read', 'write'] =
 *   ['contacts-read', contacts-write']
 * @param string aPermName
 * @param array aAccess
 * @returns array containing access-appended permission names.
 **/
this.appendAccessToPermName = function appendAccessToPermName(aPermName, aAccess) {
  if (aAccess.length == 0) {
    return [aPermName];
  }
  return aAccess.map(function(aMode) {
    return aPermName + "-" + aMode;
  });
};

/**
 * Expand an access string into multiple permission names,
 *   e.g: permission name 'contacts' with 'readwrite' =
 *   ['contacts-read', 'contacts-create', 'contacts-write']
 * @param string aPermName
 * @param string aAccess (optional)
 * @returns array containing expanded permission names.
 **/
this.expandPermissions = function expandPermissions(aPermName, aAccess) {
  if (!PermissionsTable[aPermName]) {
    Cu.reportError("PermissionsInstaller.jsm: expandPermissions: Unknown Permission: " + aPermName);
    dump("PermissionsInstaller.jsm: expandPermissions: Unknown Permission: " + aPermName);
    return [];
  }

  const tableEntry = PermissionsTable[aPermName];

  if (tableEntry.substitute && tableEntry.additional) {
    Cu.reportError("PermissionsInstaller.jsm: expandPermissions: Can't handle both 'substitute' " +
                   "and 'additional' entries for permission: " + aPermName);
    dump("PermissionsInstaller.jsm: expandPermissions: Can't handle both 'substitute' " +
         "and 'additional' entries for permission: " + aPermName);
    return [];
  }

  if (!aAccess && tableEntry.access ||
      aAccess && !tableEntry.access) {
    Cu.reportError("PermissionsTable.jsm: expandPermissions: Invalid Manifest : " +
                   aPermName + " " + aAccess + "\n");
    dump("PermissionsInstaller.jsm: expandPermissions: Invalid Manifest: " +
         aPermName + " " + aAccess + "\n");
    throw new Error("PermissionsInstaller.jsm: expandPermissions: Invalid Manifest: " +
                    aPermName + " " + aAccess + "\n");
  }

  let expandedPermNames = [];

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

    let permArr = appendAccessToPermName(aPermName, requestedSuffixes);

    // Add the same suffix to each of the additions.
    if (tableEntry.additional) {
      for each (let additional in tableEntry.additional) {
        permArr = permArr.concat(appendAccessToPermName(additional, requestedSuffixes));
      }
    }

    // Only add the suffixed version if the suffix exisits in the table.
    for (let idx in permArr) {
      let suffix = requestedSuffixes[idx % requestedSuffixes.length];
      if (tableEntry.access.indexOf(suffix) != -1) {
        expandedPermNames.push(permArr[idx]);
      }
    }
  } else if (tableEntry.substitute) {
    expandedPermNames = expandedPermNames.concat(tableEntry.substitute);
  } else {
    expandedPermNames.push(aPermName);
    // Include each of the additions exactly as they appear in the table.
    if (tableEntry.additional) {
      expandedPermNames = expandedPermNames.concat(tableEntry.additional);
    }
  }

  return expandedPermNames;
};

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
