/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [
  "PermissionsTable",
  "PermissionsReverseTable",
  "expandPermissions",
  "appendAccessToPermName",
  "isExplicitInPermissionsTable"
];

// Permission access flags
const READONLY = "readonly";
const CREATEONLY = "createonly";
const READCREATE = "readcreate";
const READWRITE = "readwrite";

const UNKNOWN_ACTION = Ci.nsIPermissionManager.UNKNOWN_ACTION;
const ALLOW_ACTION = Ci.nsIPermissionManager.ALLOW_ACTION;
const DENY_ACTION = Ci.nsIPermissionManager.DENY_ACTION;
const PROMPT_ACTION = Ci.nsIPermissionManager.PROMPT_ACTION;

// Permissions Matrix: https://docs.google.com/spreadsheet/ccc?key=0Akyz_Bqjgf5pdENVekxYRjBTX0dCXzItMnRyUU1RQ0E#gid=0

// Permissions that are implicit:
// battery-status, network-information, vibration,
// device-capabilities

this.PermissionsTable =  { geolocation: {
                             app: PROMPT_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: PROMPT_ACTION
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
                           "device-storage:crashes": {
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
                           mobilenetwork: {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           power: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           push: {
                            app: ALLOW_ACTION,
                            privileged: ALLOW_ACTION,
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
                           phonenumberservice: {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           fmradio: {
                             app: DENY_ACTION,
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
                             app: ALLOW_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION,
                             substitute: [
                               "indexedDB-unlimited",
                               "offline-app",
                               "pin-app",
                               "default-persistent-storage"
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
                           "keyboard": {
                             app: DENY_ACTION,
                             privileged: ALLOW_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "inputmethod-manage": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "wappush": {
                             app: DENY_ACTION,
                             privileged: DENY_ACTION,
                             certified: ALLOW_ACTION
                           },
                           "audio-capture": {
                             app: PROMPT_ACTION,
                             privileged: PROMPT_ACTION,
                             certified: PROMPT_ACTION
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
    let errorMsg =
      "PermissionsTable.jsm: expandPermissions: Unknown Permission: " + aPermName;
    Cu.reportError(errorMsg);
    dump(errorMsg);
    return [];
  }

  const tableEntry = PermissionsTable[aPermName];

  if (tableEntry.substitute && tableEntry.additional) {
    let errorMsg =
      "PermissionsTable.jsm: expandPermissions: Can't handle both 'substitute' " +
      "and 'additional' entries for permission: " + aPermName;
    Cu.reportError(errorMsg);
    dump(errorMsg);
    return [];
  }

  if (!aAccess && tableEntry.access ||
      aAccess && !tableEntry.access) {
    let errorMsg =
      "PermissionsTable.jsm: expandPermissions: Invalid access for permission " +
      aPermName + ": " + aAccess + "\n";
    Cu.reportError(errorMsg);
    dump(errorMsg);
    return [];
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

    // Only add the suffixed version if the suffix exists in the table.
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

this.PermissionsReverseTable = (function () {
  // PermissionsTable as it is works well for direct searches, but not
  // so well for reverse ones (that is, if I get something like
  // device-storage:music-read or indexedDB-chrome-settings-read how
  // do I know which permission it really is? Hence this table is
  // born. The idea is that
  // reverseTable[device-storage:music-read] should return
  // device-storage:music
  let reverseTable = {};

  for (let permName in PermissionsTable) {
    let permAliases;
    if (PermissionsTable[permName].access) {
      permAliases = expandPermissions(permName, "readwrite");
    } else {
      permAliases = expandPermissions(permName);
    }
    for (let i = 0; i < permAliases.length; i++) {
      reverseTable[permAliases[i]] = permName;
    }
  }

  return reverseTable;

})();

this.isExplicitInPermissionsTable = function(aPermName, aIntStatus) {

  // Check to see if the 'webapp' is app/privileged/certified.
  let appStatus;
  switch (aIntStatus) {
    case Ci.nsIPrincipal.APP_STATUS_CERTIFIED:
      appStatus = "certified";
      break;
    case Ci.nsIPrincipal.APP_STATUS_PRIVILEGED:
      appStatus = "privileged";
      break;
    default: // If it isn't certified or privileged, it's app
      appStatus = "app";
      break;
  }

  let realPerm = PermissionsReverseTable[aPermName];

  if (realPerm) {
    return (PermissionsTable[realPerm][appStatus] ==
            Ci.nsIPermissionManager.PROMPT_ACTION);
  } else {
    return false;
  }
}
