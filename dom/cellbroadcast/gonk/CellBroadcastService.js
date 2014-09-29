/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

const GONK_CELLBROADCAST_SERVICE_CONTRACTID =
  "@mozilla.org/cellbroadcast/gonkservice;1";
const GONK_CELLBROADCAST_SERVICE_CID =
  Components.ID("{7ba407ce-21fd-11e4-a836-1bfdee377e5c}");
const CELLBROADCASTMESSAGE_CID =
  Components.ID("{29474c96-3099-486f-bb4a-3c9a1da834e4}");
const CELLBROADCASTETWSINFO_CID =
  Components.ID("{59f176ee-9dcd-4005-9d47-f6be0cd08e17}");

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

let DEBUG;
function debug(s) {
  dump("CellBroadcastService: " + s);
}

function CellBroadcastService() {
  this._listeners = [];

  this._updateDebugFlag();

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}
CellBroadcastService.prototype = {
  classID: GONK_CELLBROADCAST_SERVICE_CID,

  classInfo: XPCOMUtils.generateCI({classID: GONK_CELLBROADCAST_SERVICE_CID,
                                    contractID: GONK_CELLBROADCAST_SERVICE_CONTRACTID,
                                    classDescription: "CellBroadcastService",
                                    interfaces: [Ci.nsICellBroadcastService,
                                                 Ci.nsIGonkCellBroadcastService],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICellBroadcastService,
                                         Ci.nsIGonkCellBroadcastService,
                                         Ci.nsIObserver]),

  // An array of nsICellBroadcastListener instances.
  _listeners: null,

  _updateDebugFlag: function() {
    try {
      DEBUG = RIL.DEBUG_RIL ||
              Services.prefs.getBoolPref(kPrefRilDebuggingEnabled);
    } catch (e) {}
  },

  _convertCbGsmGeographicalScope: function(aGeographicalScope) {
    return (aGeographicalScope >= Ci.nsICellBroadcastService.GSM_GEOGRAPHICAL_SCOPE_INVALID)
      ? null
      : RIL.CB_GSM_GEOGRAPHICAL_SCOPE_NAMES[aGeographicalScope];
  },

  _convertCbMessageClass: function(aMessageClass) {
    return (aMessageClass >= Ci.nsICellBroadcastService.GSM_MESSAGE_CLASS)
      ? null
      : RIL.GECKO_SMS_MESSAGE_CLASSES[aMessageClass];
  },

  _convertCbEtwsWarningType: function(aWarningType) {
    return (aWarningType >= Ci.nsICellBroadcastService.GSM_ETWS_WARNING_INVALID)
      ? null
      : RIL.CB_ETWS_WARNING_TYPE_NAMES[aWarningType];
  },

  /**
   * nsICellBroadcastService interface
   */
  registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.push(aListener);
  },

  unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);

    if (index < 0) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    this._listeners.splice(index, 1);
  },

  /**
   * nsIGonkCellBroadcastService interface
   */
  notifyMessageReceived: function(aServiceId,
                                  aGsmGeographicalScope,
                                  aMessageCode,
                                  aMessageId,
                                  aLanguage,
                                  aBody,
                                  aMessageClass,
                                  aTimestamp,
                                  aCdmaServiceCategory,
                                  aHasEtwsInfo,
                                  aEtwsWarningType,
                                  aEtwsEmergencyUserAlert,
                                  aEtwsPopup) {
    // Broadcast CBS System message
    // Align the same layout to MozCellBroadcastMessage
    let systemMessage = {
      serviceId: aServiceId,
      gsmGeographicalScope: this._convertCbGsmGeographicalScope(aGsmGeographicalScope),
      messageCode: aMessageCode,
      messageId: aMessageId,
      language: aLanguage,
      body: aBody,
      messageClass: this._convertCbMessageClass(aMessageClass),
      timestamp: aTimestamp,
      cdmaServiceCategory: null,
      etws: null
    };

    if (aHasEtwsInfo) {
      systemMessage.etws = {
        warningType: this._convertCbEtwsWarningType(aEtwsWarningType),
        emergencyUserAlert: aEtwsEmergencyUserAlert,
        popup: aEtwsPopup
      };
    }

    if (aCdmaServiceCategory !=
        Ci.nsICellBroadcastService.CDMA_SERVICE_CATEGORY_INVALID) {
      systemMessage.cdmaServiceCategory = aCdmaServiceCategory;
    }

    if (DEBUG) {
      debug("CBS system message to be broadcasted: " + JSON.stringify(systemMessage));
    }

    gSystemMessenger.broadcastMessage("cellbroadcast-received", systemMessage);

    // Notify received message to registered listener
    for (let listener of this._listeners) {
      try {
        listener.notifyMessageReceived(aServiceId,
                                       aGsmGeographicalScope,
                                       aMessageCode,
                                       aMessageId,
                                       aLanguage,
                                       aBody,
                                       aMessageClass,
                                       aTimestamp,
                                       aCdmaServiceCategory,
                                       aHasEtwsInfo,
                                       aEtwsWarningType,
                                       aEtwsEmergencyUserAlert,
                                       aEtwsPopup);
      } catch (e) {
        debug("listener threw an exception: " + e);
      }
    }
  },

  /**
   * nsIObserver interface.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

        // Remove all listeners.
        this._listeners = [];
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CellBroadcastService]);
