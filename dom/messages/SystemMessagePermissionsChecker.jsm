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

XPCOMUtils.defineLazyServiceGetter(this, "dataStoreService",
                                   "@mozilla.org/datastore-service;1",
                                   "nsIDataStoreService");

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
  "cellbroadcast-received": {
    "cellbroadcast": []
  },
  "connection": { },
  "captive-portal": {
    "wifi-manage": []
  },
  "dummy-system-message": { }, // for system message testing framework
  "dummy-system-message2": { }, // for system message testing framework
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
  "request-sync": { },
  "sms-delivery-success": {
    "sms": []
  },
  "sms-delivery-error": {
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
  "sms-failed": {
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
  "nfc-hci-event-transaction": {
    "nfc-hci-events": []
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
  "wifip2p-pairing-request": {
    "wifi-manage": []
  },
  "first-run-with-sim": {
    "settings": ["read", "write"]
  },
  "audiochannel-interruption-begin" : {},
  "audiochannel-interruption-ended" : {}
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
            "Please add them to the SystemMessage[Prefix]PermissionsTable.");
      return null;
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
   * Check if the message is a datastore-update message
   * @param string aSysMsgName
   *        The system messsage name.
   */
  isDataStoreSystemMessage: function(aSysMsgName) {
    return aSysMsgName.indexOf('datastore-update-') === 0;
  },

  /**
   * Check if this manifest can deliver this particular datastore message.
   */
  canDeliverDataStoreSystemMessage: function(aSysMsgName, aManifestURL) {
    let store = aSysMsgName.substr('datastore-update-'.length);

    // Get all the manifest URLs of the apps which can access the datastore.
    let manifestURLs = dataStoreService.getAppManifestURLsForDataStore(store);
    let enumerate = manifestURLs.enumerate();
    while (enumerate.hasMoreElements()) {
      let manifestURL = enumerate.getNext().QueryInterface(Ci.nsISupportsString);
      if (manifestURL == aManifestURL) {
        return true;
      }
    }

    return false;
  },

  /**
   * Check if the system message is permitted to be registered for the given
   * app at start-up based on the permissions claimed in the app's manifest.
   * @param string aSysMsgName
   *        The system messsage name.
   * @param string aManifestURL
   *        The app's manifest URL.
   * @param string aOrigin
   *        The app's origin.
   * @param object aManifest
   *        The app's manifest.
   * @returns bool
   *        Is permitted or not.
   **/
  isSystemMessagePermittedToRegister: function (aSysMsgName,
                                                aManifestURL,
                                                aOrigin,
                                                aManifest) {
    // Test if the launch path of the app has the right permission.
    let newManifest = new ManifestHelper(aManifest, aOrigin, aManifestURL);
    let launchUrl = newManifest.fullLaunchPath();
    return this.isSystemMessagePermittedToSend(aSysMsgName,
                                               launchUrl,
                                               aManifestURL);
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

    if (this.isDataStoreSystemMessage(aSysMsgName) &&
        this.canDeliverDataStoreSystemMessage(aSysMsgName, aManifestURL)) {
      return true;
    }

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
