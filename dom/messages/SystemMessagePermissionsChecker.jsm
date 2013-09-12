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
                         "SystemMessagePermissionsTable"];

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
  "headset-button": { },
  "icc-stkcommand": {
    "settings": ["read", "write"]
  },
  "media-button": { },
  "notification": {
    "desktop-notification": []
  },
  "push": {
  	"push": []
  },
  "push-register": {
  	"push": []
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
      debug("'" + aSysMsgName + "' is not associated with permissions. " +
            "Please add them to the SystemMessagePermissionsTable.");
      return null;
    }

    let object = { };
    for (let permName in permNames) {
      if (PermissionsTable[permName] === undefined) {
        debug("'" + permName + "' for '" + aSysMsgName + "' is invalid. " +
              "Please correct it in the SystemMessagePermissionsTable.");
        return null;
      }

      // Construct a new permission name array by adding the access suffixes.
      let access = permNames[permName];
      if (!access || !Array.isArray(access)) {
        debug("'" + permName + "' is not associated with access array. " +
              "Please correct it in the SystemMessagePermissionsTable.");
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
   * @param string aOrigin
   *        The app's origin.
   * @param object aManifest
   *        The app's manifest.
   * @returns bool
   *        Is permitted or not.
   **/
  isSystemMessagePermittedToRegister:
    function isSystemMessagePermittedToRegister(aSysMsgName, aOrigin, aManifest) {
    debug("isSystemMessagePermittedToRegister(): " +
          "aSysMsgName: " + aSysMsgName + ", " +
          "aOrigin: " + aOrigin + ", " +
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

    let newManifest = new ManifestHelper(aManifest, aOrigin);

    for (let permName in permNames) {
      // The app doesn't claim valid permissions for this sytem message.
      if (!newManifest.permissions || !newManifest.permissions[permName]) {
        debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
              "Please add the permission for app: '" + aOrigin + "'.");
        return false;
      }
      let permValue = PermissionsTable[permName][appStatus];
      if (permValue != Ci.nsIPermissionManager.PROMPT_ACTION &&
          permValue != Ci.nsIPermissionManager.ALLOW_ACTION) {
        debug("'" + aSysMsgName + "' isn't permitted by '" + permName + "'. " +
              "Please add the permission for app: '" + aOrigin + "'.");
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
   * @param string aPageURI
   *        The app's page URI.
   * @param string aManifestURL
   *        The app's manifest URL.
   * @returns bool
   *        Is permitted or not.
   **/
  isSystemMessagePermittedToSend:
    function isSystemMessagePermittedToSend(aSysMsgName, aPageURI, aManifestURL) {
    debug("isSystemMessagePermittedToSend(): " +
          "aSysMsgName: " + aSysMsgName + ", " +
          "aPageURI: " + aPageURI + ", " +
          "aManifestURL: " + aManifestURL);

    let permNames = this.getSystemMessagePermissions(aSysMsgName);
    if (permNames === null) {
      return false;
    }

    let pageURI = Services.io.newURI(aPageURI, null, null);
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
