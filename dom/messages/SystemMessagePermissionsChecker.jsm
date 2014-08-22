/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/PermissionsInstaller.jsm");
Cu.import("resource://gre/modules/PermissionsTable.jsm");
Cu.import("resource://gre/modules/PermissionSettings.jsm");

this.EXPORTED_SYMBOLS = ["SystemMessagePermissionsChecker",
                         "SystemMessagePermissionsTable",
                         "SystemMessagePrefixPermissionsTable"];

function debug(aStr) {
  // dump("SystemMessagePermissionsChecker.jsm: " + aStr + "\n");
}

// This table maps system message to permission(s), indicating only
// the system messages granted by the page's permissions are allowed
// to be registered or sent to that page. Note the empty permission
// set means this type of system message is always permitted.

this.SystemMessagePermissionsTable = {
  "activity": { },
  "alarm": {
    "alarms": []
  },
  "bluetooth-dialer-command": {
    "telephony": []
  },
  "bluetooth-cancel": {
    "bluetooth": []
  },
  "bluetooth-hid-status-changed": {
    "bluetooth": []
  },
  "bluetooth-pairing-request": {
    "bluetooth": []
  },
  "bluetooth-opp-transfer-complete": {
    "bluetooth": []
  },
  "bluetooth-opp-update-progress": {
    "bluetooth": []
  },
  "bluetooth-opp-receiving-file-confirmation": {
    "bluetooth": []
  },
  "bluetooth-opp-transfer-start": {
    "bluetooth": []
  },
  "cellbroadcast-received": {
    "cellbroadcast": []
  },
  "connection": { },
  "captive-portal": {
    "wifi-manage": []
  },
  "dummy-system-message": { }, // for system message testing framework
  "headset-button": { },
  "icc-stkcommand": {
    "settings": ["read", "write"]
  },
  "media-button": { },
  "networkstats-alarm": {
    "networkstats-manage": []
  },
  "notification": {
    "desktop-notification": []
  },
  "push": {
  	"push": []
  },
  "push-register": {
  	"push": []
  },
  "sms-delivery-success": {
    "sms": []
  },
  "sms-read-success": {
    "sms": []
  },
  "sms-received": {
    "sms": []
  },
  "sms-sent": {
    "sms": []
  },
  "telephony-new-call": {
    "telephony": []
  },
  "telephony-call-ended": {
    "telephony": []
  },
  "ussd-received": {
    "mobileconnection": []
  },
  "wappush-received": {
    "wappush": []
  },
  "cdma-info-rec-received": {
    "mobileconnection": []
  },
  "nfc-manager-tech-discovered": {
    "nfc-manager": []
  },
  "nfc-manager-tech-lost": {
    "nfc-manager": []
  },
  "nfc-manager-send-file": {
    "nfc-manager": []
  },
  "nfc-powerlevel-change": {
    "settings": ["read", "write"]
  },
  "wifip2p-pairing-request": { },
  "first-run-with-sim": {
    "settings": ["read", "write"]
  }
};

// This table maps system message prefix to permission(s), indicating only
// the system messages with specified prefixes granted by the page's permissions
// are allowed to be registered or sent to that page. Note the empty permission
// set means this type of system message is always permitted.
//
// Note that this table is only used when the permission checker can't find a
// match in SystemMessagePermissionsTable listed above.

this.SystemMessagePrefixPermissionsTable = {
  "datastore-update-": { },
};

this.SystemMessagePermissionsChecker = {
  /**
   * Return all the needed permission names for the given system message.
   * @param string aSysMsgName
   *        The system messsage name.
   * @returns object
   *        Format: { permName (string): permNamesWithAccess (string array), ... }
   *        Ex, { "settings": ["settings-read", "settings-write"], ... }.
   *        Note: an empty object will be returned if it's always permitted.
   * @returns null
   *        Return and report error when any unexpected error is ecountered.
   *        Ex, when the system message we want to search is not included.
   **/
  getSystemMessagePermissions: function getSystemMessagePermissions(aSysMsgName) {
    debug("getSystemMessagePermissions(): aSysMsgName: " + aSysMsgName);

    let permNames = SystemMessagePermissionsTable[aSysMsgName];
    if (permNames === undefined) {
      // Try to look up in the prefix table.
      for (let sysMsgPrefix in SystemMessagePrefixPermissionsTable) {
        if (aSysMsgName.indexOf(sysMsgPrefix) === 0) {
          permNames = SystemMessagePrefixPermissionsTable[sysMsgPrefix];
          break;
        }
      }

      if (permNames === undefined) {
        debug("'" + aSysMsgName + "' is not associated with permissions. " +
              "Please add them to the SystemMessage[Prefix]PermissionsTable.");
        return null;
      }
    }

    let object = { };
    for (let permName in permNames) {
      if (PermissionsTable[permName] === undefined) {
        debug("'" + permName + "' for '" + aSysMsgName + "' is invalid. " +
              "Please correct it in the SystemMessage[Prefix]PermissionsTable.");
        return null;
      }

      // Construct a new permission name array by adding the access suffixes.
      let access = permNames[permName];
      if (!access || !Array.isArray(access)) {
        debug("'" + permName + "' is not associated with access array. " +
              "Please correct it in the SystemMessage[Prefix]PermissionsTable.");
        return null;
      }
      object[permName] = appendAccessToPermName(permName, access);
    }
    return object
  },

  /**
   * Check if the system message is permitted to be registered for the given
   * app at start-up based on the permissions claimed in the app's manifest.
   * @param string aSysMsgName
   *        The system messsage name.
   * @param string aManifestURL
   *        The app's manifest URL.
   * @param object aManifest
   *        The app's manifest.
   * @returns bool
   *        Is permitted or not.
   **/
  isSystemMessagePermittedToRegister:
    function isSystemMessagePermittedToRegister(aSysMsgName,
                                                aManifestURL,
                                                aManifest) {
    debug("isSystemMessagePermittedToRegister(): " +
          "aSysMsgName: " + aSysMsgName + ", " +
          "aManifestURL: " + aManifestURL + ", " +
          "aManifest: " + JSON.stringify(aManifest));

    let permNames = this.getSystemMessagePermissions(aSysMsgName);
    if (permNames === null) {
      return false;
    }

    // Check to see if the 'webapp' is app/privileged/certified.
    let appStatus;
    switch (AppsUtils.getAppManifestStatus(aManifest)) {
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
      throw new Error("SystemMessagePermissionsChecker.jsm: " +
                      "Cannot decide the app's status. Install cancelled.");
      break;
    }

    // It's ok here to not pass the origin to ManifestHelper since we only
    // need the permission property and that doesn't depend on uri resolution.
    let newManifest = new ManifestHelper(aManifest, aManifestURL, aManifestURL);

    for (let permName in permNames) {
      // The app doesn't claim valid permissions for this sytem message.
      if (!newManifest.permissions || !newManifest.permissions[permName]) {
        debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
              "Please add the permission for app: '" + aManifestURL + "'.");
        return false;
      }
      let permValue = PermissionsTable[permName][appStatus];
      if (permValue != Ci.nsIPermissionManager.PROMPT_ACTION &&
          permValue != Ci.nsIPermissionManager.ALLOW_ACTION) {
        debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
              "Please add the permission for app: '" + aManifestURL + "'.");
        return false;
      }

      // Compare the expanded permission names between the ones in
      // app's manifest and the ones needed for system message.
      let expandedPermNames =
        expandPermissions(permName,
                          newManifest.permissions[permName].access);

      let permNamesWithAccess = permNames[permName];

      // Early return false as soon as any permission is not matched.
      for (let idx in permNamesWithAccess) {
        let index = expandedPermNames.indexOf(permNamesWithAccess[idx]);
        if (index == -1) {
          debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
                "Please add the permission for app: '" + aOrigin + "'.");
          return false;
        }
      }
    }

    // All the permissions needed for this system message are matched.
    return true;
  },

  /**
   * Check if the system message is permitted to be sent to the given
   * app's page at run-time based on the current app's permissions.
   * @param string aSysMsgName
   *        The system messsage name.
   * @param string aPageURL
   *        The app's page URL.
   * @param string aManifestURL
   *        The app's manifest URL.
   * @returns bool
   *        Is permitted or not.
   **/
  isSystemMessagePermittedToSend:
    function isSystemMessagePermittedToSend(aSysMsgName, aPageURL, aManifestURL) {
    debug("isSystemMessagePermittedToSend(): " +
          "aSysMsgName: " + aSysMsgName + ", " +
          "aPageURL: " + aPageURL + ", " +
          "aManifestURL: " + aManifestURL);

    let permNames = this.getSystemMessagePermissions(aSysMsgName);
    if (permNames === null) {
      return false;
    }

    let pageURI = Services.io.newURI(aPageURL, null, null);
    for (let permName in permNames) {
      let permNamesWithAccess = permNames[permName];

      // Early return false as soon as any permission is not matched.
      for (let idx in permNamesWithAccess) {
        if(PermissionSettingsModule.getPermission(permNamesWithAccess[idx],
                                                  aManifestURL,
                                                  pageURI.prePath,
                                                  false) != "allow") {
          debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
                "Please add the permission for app: '" + pageURI.prePath + "'.");
          return false;
        }
      }
    }

    // All the permissions needed for this system message are matched.
    return true;
  }
};
