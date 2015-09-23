/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/systemlibs.js");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "gNetworkManager",
                                   "@mozilla.org/network/manager;1",
                                   "nsINetworkManager");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIMobileConnectionService");

XPCOMUtils.defineLazyServiceGetter(this, "gIccService",
                                   "@mozilla.org/icc/iccservice;1",
                                   "nsIIccService");

XPCOMUtils.defineLazyServiceGetter(this, "gDataCallInterfaceService",
                                   "@mozilla.org/datacall/interfaceservice;1",
                                   "nsIDataCallInterfaceService");

XPCOMUtils.defineLazyGetter(this, "RIL", function() {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

// Ril quirk to attach data registration on demand.
var RILQUIRKS_DATA_REGISTRATION_ON_DEMAND =
  libcutils.property_get("ro.moz.ril.data_reg_on_demand", "false") == "true";

// Ril quirk to control the uicc/data subscription.
var RILQUIRKS_SUBSCRIPTION_CONTROL =
  libcutils.property_get("ro.moz.ril.subscription_control", "false") == "true";

// Ril quirk to enable IPv6 protocol/roaming protocol in APN settings.
var RILQUIRKS_HAVE_IPV6 =
  libcutils.property_get("ro.moz.ril.ipv6", "false") == "true";

const DATACALLMANAGER_CID =
  Components.ID("{35b9efa2-e42c-45ce-8210-0a13e6f4aadc}");
const DATACALLHANDLER_CID =
  Components.ID("{132b650f-c4d8-4731-96c5-83785cb31dee}");
const RILNETWORKINTERFACE_CID =
  Components.ID("{9574ee84-5d0d-4814-b9e6-8b279e03dcf4}");
const RILNETWORKINFO_CID =
  Components.ID("{dd6cf2f0-f0e3-449f-a69e-7c34fdcb8d4b}");

const TOPIC_XPCOM_SHUTDOWN      = "xpcom-shutdown";
const TOPIC_MOZSETTINGS_CHANGED = "mozsettings-changed";
const TOPIC_PREF_CHANGED        = "nsPref:changed";
const TOPIC_DATA_CALL_ERROR     = "data-call-error";
const PREF_RIL_DEBUG_ENABLED    = "ril.debugging.enabled";

const NETWORK_TYPE_UNKNOWN     = Ci.nsINetworkInfo.NETWORK_TYPE_UNKNOWN;
const NETWORK_TYPE_WIFI        = Ci.nsINetworkInfo.NETWORK_TYPE_WIFI;
const NETWORK_TYPE_MOBILE      = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE;
const NETWORK_TYPE_MOBILE_MMS  = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_MMS;
const NETWORK_TYPE_MOBILE_SUPL = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_SUPL;
const NETWORK_TYPE_MOBILE_IMS  = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_IMS;
const NETWORK_TYPE_MOBILE_DUN  = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_DUN;
const NETWORK_TYPE_MOBILE_FOTA = Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE_FOTA;

const NETWORK_STATE_UNKNOWN       = Ci.nsINetworkInfo.NETWORK_STATE_UNKNOWN;
const NETWORK_STATE_CONNECTING    = Ci.nsINetworkInfo.NETWORK_STATE_CONNECTING;
const NETWORK_STATE_CONNECTED     = Ci.nsINetworkInfo.NETWORK_STATE_CONNECTED;
const NETWORK_STATE_DISCONNECTING = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTING;
const NETWORK_STATE_DISCONNECTED  = Ci.nsINetworkInfo.NETWORK_STATE_DISCONNECTED;

const INT32_MAX = 2147483647;

// set to true in ril_consts.js to see debug messages
var DEBUG = RIL.DEBUG_RIL;

function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref;
  try {
    debugPref = Services.prefs.getBoolPref(PREF_RIL_DEBUG_ENABLED);
  } catch (e) {
    debugPref = false;
  }
  DEBUG = debugPref || RIL.DEBUG_RIL;
}
updateDebugFlag();

function DataCallManager() {
  this._connectionHandlers = [];

  let numRadioInterfaces = gMobileConnectionService.numItems;
  for (let clientId = 0; clientId < numRadioInterfaces; clientId++) {
    this._connectionHandlers.push(new DataCallHandler(clientId));
  }

  let lock = gSettingsService.createLock();
  // Read the APN data from the settings DB.
  lock.get("ril.data.apnSettings", this);
  // Read the data enabled setting from DB.
  lock.get("ril.data.enabled", this);
  lock.get("ril.data.roaming_enabled", this);
  // Read the default client id for data call.
  lock.get("ril.data.defaultServiceId", this);

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
  Services.obs.addObserver(this, TOPIC_MOZSETTINGS_CHANGED, false);
  Services.prefs.addObserver(PREF_RIL_DEBUG_ENABLED, this, false);
}
DataCallManager.prototype = {
  classID:   DATACALLMANAGER_CID,
  classInfo: XPCOMUtils.generateCI({classID: DATACALLMANAGER_CID,
                                    classDescription: "Data Call Manager",
                                    interfaces: [Ci.nsIDataCallManager]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallManager,
                                         Ci.nsIObserver,
                                         Ci.nsISettingsServiceCallback]),

  _connectionHandlers: null,

  // Flag to determine the data state to start with when we boot up. It
  // corresponds to the 'ril.data.enabled' setting from the UI.
  _dataEnabled: false,

  // Flag to record the default client id for data call. It corresponds to
  // the 'ril.data.defaultServiceId' setting from the UI.
  _dataDefaultClientId: -1,

  // Flag to record the current default client id for data call.
  // It differs from _dataDefaultClientId in that it is set only when
  // the switch of client id process is done.
  _currentDataClientId: -1,

  // Pending function to execute when we are notified that another data call has
  // been disconnected.
  _pendingDataCallRequest: null,

  debug: function(aMsg) {
    dump("-*- DataCallManager: " + aMsg + "\n");
  },

  get dataDefaultServiceId() {
    return this._dataDefaultClientId;
  },

  getDataCallHandler: function(aClientId) {
    let handler = this._connectionHandlers[aClientId]
    if (!handler) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return handler;
  },

  _setDataRegistration: function(aDataCallInterface, aAttach) {
    return new Promise(function(aResolve, aReject) {
      let callback = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallCallback]),
        notifySuccess: function() {
          aResolve();
        },
        notifyError: function(aErrorMsg) {
          aReject(aErrorMsg);
        }
      };

      aDataCallInterface.setDataRegistration(aAttach, callback);
    });
  },

  _handleDataClientIdChange: function(aNewClientId) {
    if (this._dataDefaultClientId === aNewClientId) {
       return;
    }
    this._dataDefaultClientId = aNewClientId;

    // This is to handle boot up stage.
    if (this._currentDataClientId == -1) {
      this._currentDataClientId = this._dataDefaultClientId;
      let connHandler = this._connectionHandlers[this._currentDataClientId];
      let dcInterface = connHandler.dataCallInterface;
      if (RILQUIRKS_DATA_REGISTRATION_ON_DEMAND ||
          RILQUIRKS_SUBSCRIPTION_CONTROL) {
        this._setDataRegistration(dcInterface, true);
      }
      if (this._dataEnabled) {
        let settings = connHandler.dataCallSettings;
        settings.oldEnabled = settings.enabled;
        settings.enabled = true;
        connHandler.updateRILNetworkInterface();
      }
      return;
    }

    let oldConnHandler = this._connectionHandlers[this._currentDataClientId];
    let oldIface = oldConnHandler.dataCallInterface;
    let oldSettings = oldConnHandler.dataCallSettings;
    let newConnHandler = this._connectionHandlers[this._dataDefaultClientId];
    let newIface = newConnHandler.dataCallInterface;
    let newSettings = newConnHandler.dataCallSettings;

    let applyPendingDataSettings = () => {
      if (DEBUG) {
        this.debug("Apply pending data registration and settings.");
      }

      if (RILQUIRKS_DATA_REGISTRATION_ON_DEMAND ||
          RILQUIRKS_SUBSCRIPTION_CONTROL) {
        this._setDataRegistration(oldIface, false).then(() => {
          if (this._dataEnabled) {
            newSettings.oldEnabled = newSettings.enabled;
            newSettings.enabled = true;
          }
          this._currentDataClientId = this._dataDefaultClientId;

          this._setDataRegistration(newIface, true).then(() => {
            newConnHandler.updateRILNetworkInterface();
          });
        });
        return;
      }

      if (this._dataEnabled) {
        newSettings.oldEnabled = newSettings.enabled;
        newSettings.enabled = true;
      }

      this._currentDataClientId = this._dataDefaultClientId;
      newConnHandler.updateRILNetworkInterface();
    };

    if (this._dataEnabled) {
      oldSettings.oldEnabled = oldSettings.enabled;
      oldSettings.enabled = false;
    }

    oldConnHandler.deactivateDataCallsAndWait().then(() => {
      applyPendingDataSettings();
    });
  },

  _shutdown: function() {
    for (let handler of this._connectionHandlers) {
      handler.shutdown();
    }
    this._connectionHandlers = null;
    Services.prefs.removeObserver(PREF_RIL_DEBUG_ENABLED, this);
    Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
    Services.obs.removeObserver(this, TOPIC_MOZSETTINGS_CHANGED);
  },

  /**
   * nsISettingsServiceCallback
   */
  handle: function(aName, aResult) {
    switch (aName) {
      case "ril.data.apnSettings":
        if (DEBUG) {
          this.debug("'ril.data.apnSettings' is now " +
                     JSON.stringify(aResult));
        }
        if (!aResult) {
          break;
        }
        for (let clientId in this._connectionHandlers) {
          let handler = this._connectionHandlers[clientId];
          let apnSetting = aResult[clientId];
          if (handler && apnSetting) {
            handler.updateApnSettings(apnSetting);
          }
        }
        break;
      case "ril.data.enabled":
        if (DEBUG) {
          this.debug("'ril.data.enabled' is now " + aResult);
        }
        if (this._dataEnabled === aResult) {
          break;
        }
        this._dataEnabled = aResult;

        if (DEBUG) {
          this.debug("Default id for data call: " + this._dataDefaultClientId);
        }
        if (this._dataDefaultClientId === -1) {
          // We haven't got the default id for data from db.
          break;
        }

        let connHandler = this._connectionHandlers[this._dataDefaultClientId];
        let settings = connHandler.dataCallSettings;
        settings.oldEnabled = settings.enabled;
        settings.enabled = aResult;
        connHandler.updateRILNetworkInterface();
        break;
      case "ril.data.roaming_enabled":
        if (DEBUG) {
          this.debug("'ril.data.roaming_enabled' is now " + aResult);
          this.debug("Default id for data call: " + this._dataDefaultClientId);
        }
        for (let clientId = 0; clientId < this._connectionHandlers.length; clientId++) {
          let connHandler = this._connectionHandlers[clientId];
          let settings = connHandler.dataCallSettings;
          settings.roamingEnabled = Array.isArray(aResult) ? aResult[clientId]
                                                           : aResult;
        }
        if (this._dataDefaultClientId === -1) {
          // We haven't got the default id for data from db.
          break;
        }
        this._connectionHandlers[this._dataDefaultClientId].updateRILNetworkInterface();
        break;
      case "ril.data.defaultServiceId":
        aResult = aResult || 0;
        if (DEBUG) {
          this.debug("'ril.data.defaultServiceId' is now " + aResult);
        }
        this._handleDataClientIdChange(aResult);
        break;
    }
  },

  handleError: function(aErrorMessage) {
    if (DEBUG) {
      this.debug("There was an error while reading RIL settings.");
    }
  },

  /**
   * nsIObserver interface methods.
   */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_MOZSETTINGS_CHANGED:
        if ("wrappedJSObject" in aSubject) {
          aSubject = aSubject.wrappedJSObject;
        }
        this.handle(aSubject.key, aSubject.value);
        break;
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        this._shutdown();
        break;
    }
  },
};

function DataCallHandler(aClientId) {
  // Initial owning attributes.
  this.clientId = aClientId;
  this.dataCallSettings = {
    oldEnabled: false,
    enabled: false,
    roamingEnabled: false
  };
  this._dataCalls = [];
  this._listeners = [];

  // This map is used to collect all the apn types and its corresponding
  // RILNetworkInterface.
  this.dataNetworkInterfaces = new Map();

  this.dataCallInterface = gDataCallInterfaceService.getDataCallInterface(aClientId);
  this.dataCallInterface.registerListener(this);

  let mobileConnection = gMobileConnectionService.getItemByServiceId(aClientId);
  mobileConnection.registerListener(this);

  this._dataInfo = {
    state: mobileConnection.data.state,
    type: mobileConnection.data.type,
    roaming: mobileConnection.data.roaming
  }
}
DataCallHandler.prototype = {
  classID:   DATACALLHANDLER_CID,
  classInfo: XPCOMUtils.generateCI({classID: DATACALLHANDLER_CID,
                                    classDescription: "Data Call Handler",
                                    interfaces: [Ci.nsIDataCallHandler]}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallHandler,
                                         Ci.nsIDataCallInterfaceListener,
                                         Ci.nsIMobileConnectionListener]),

  clientId: 0,
  dataCallInterface: null,
  dataCallSettings: null,
  dataNetworkInterfaces: null,
  _dataCalls: null,
  _dataInfo: null,

  // Apn settings to be setup after data call are cleared.
  _pendingApnSettings: null,

  debug: function(aMsg) {
    dump("-*- DataCallHandler[" + this.clientId + "]: " + aMsg + "\n");
  },

  shutdown: function() {
    // Shutdown all RIL network interfaces
    this.dataNetworkInterfaces.forEach(function(networkInterface) {
      gNetworkManager.unregisterNetworkInterface(networkInterface);
      networkInterface.shutdown();
      networkInterface = null;
    });
    this.dataNetworkInterfaces.clear();
    this._dataCalls = [];
    this.clientId = null;

    this.dataCallInterface.unregisterListener(this);
    this.dataCallInterface = null;

    let mobileConnection =
      gMobileConnectionService.getItemByServiceId(this.clientId);
    mobileConnection.unregisterListener(this);
  },

  /**
   * Check if we get all necessary APN data.
   */
  _validateApnSetting: function(aApnSetting) {
    return (aApnSetting &&
            aApnSetting.apn &&
            aApnSetting.types &&
            aApnSetting.types.length);
  },

  _convertApnType: function(aApnType) {
    switch (aApnType) {
      case "default":
        return NETWORK_TYPE_MOBILE;
      case "mms":
        return NETWORK_TYPE_MOBILE_MMS;
      case "supl":
        return NETWORK_TYPE_MOBILE_SUPL;
      case "ims":
        return NETWORK_TYPE_MOBILE_IMS;
      case "dun":
        return NETWORK_TYPE_MOBILE_DUN;
      case "fota":
        return NETWORK_TYPE_MOBILE_FOTA;
      default:
        return NETWORK_TYPE_UNKNOWN;
     }
  },

  _compareDataCallOptions: function(aDataCall, aNewDataCall) {
    return aDataCall.apnProfile.apn == aNewDataCall.apnProfile.apn &&
           aDataCall.apnProfile.user == aNewDataCall.apnProfile.user &&
           aDataCall.apnProfile.password == aNewDataCall.apnProfile.passwd &&
           aDataCall.apnProfile.authType == aNewDataCall.apnProfile.authType &&
           aDataCall.apnProfile.protocol == aNewDataCall.apnProfile.protocol &&
           aDataCall.apnProfile.roaming_protocol == aNewDataCall.apnProfile.roaming_protocol;
  },

  /**
   * This function will do the following steps:
   *   1. Clear the cached APN settings in the RIL.
   *   2. Combine APN, user name, and password as the key of |byApn| object to
   *      refer to the corresponding APN setting.
   *   3. Use APN type as the index of |byType| object to refer to the
   *      corresponding APN setting.
   *   4. Create RilNetworkInterface for each APN setting created at step 2.
   */
  _setupApnSettings: function(aNewApnSettings) {
    if (!aNewApnSettings) {
      return;
    }
    if (DEBUG) this.debug("setupApnSettings: " + JSON.stringify(aNewApnSettings));

    // Shutdown all network interfaces and clear data calls.
    this.dataNetworkInterfaces.forEach(function(networkInterface) {
      gNetworkManager.unregisterNetworkInterface(networkInterface);
      networkInterface.shutdown();
      networkInterface = null;
    });
    this.dataNetworkInterfaces.clear();
    this._dataCalls = [];

    // Cache the APN settings by APNs and by types in the RIL.
    for (let inputApnSetting of aNewApnSettings) {
      if (!this._validateApnSetting(inputApnSetting)) {
        continue;
      }

      // Use APN type as the key of dataNetworkInterfaces to refer to the
      // corresponding RILNetworkInterface.
      for (let i = 0; i < inputApnSetting.types.length; i++) {
        let apnType = inputApnSetting.types[i];
        let networkType = this._convertApnType(apnType);
        if (networkType === NETWORK_TYPE_UNKNOWN) {
          if (DEBUG) this.debug("Invalid apn type: " + apnType);
          continue;
        }

        if (DEBUG) this.debug("Preparing RILNetworkInterface for type: " + apnType);
        // Create DataCall for RILNetworkInterface or reuse one that is shareable.
        let dataCall;
        for (let i = 0; i < this._dataCalls.length; i++) {
          if (this._dataCalls[i].canHandleApn(inputApnSetting)) {
            if (DEBUG) this.debug("Found shareable DataCall, reusing it.");
            dataCall = this._dataCalls[i];
            break;
          }
        }

        if (!dataCall) {
          if (DEBUG) this.debug("No shareable DataCall found, creating one.");
          dataCall = new DataCall(this.clientId, inputApnSetting, this);
          this._dataCalls.push(dataCall);
        }

        try {
          let networkInterface = new RILNetworkInterface(this, networkType,
                                                         inputApnSetting,
                                                         dataCall);
          gNetworkManager.registerNetworkInterface(networkInterface);
          this.dataNetworkInterfaces.set(networkType, networkInterface);
        } catch (e) {
          if (DEBUG) {
            this.debug("Error setting up RILNetworkInterface for type " +
                        apnType + ": " + e);
          }
        }
      }
    }
  },

  /**
   * Check if all data is disconnected.
   */
  allDataDisconnected: function() {
    for (let i = 0; i < this._dataCalls.length; i++) {
      let dataCall = this._dataCalls[i];
      if (dataCall.state != NETWORK_STATE_UNKNOWN &&
          dataCall.state != NETWORK_STATE_DISCONNECTED) {
        return false;
      }
    }
    return true;
  },

  deactivateDataCallsAndWait: function() {
    return new Promise((aResolve, aReject) => {
      this.deactivateDataCalls({
        notifyDataCallsDisconnected: function() {
          aResolve();
        }
      });
    });
  },

  updateApnSettings: function(aNewApnSettings) {
    if (!aNewApnSettings) {
      return;
    }
    if (this._pendingApnSettings) {
      // Change of apn settings in process, just update to the newest.
      this._pengingApnSettings = aNewApnSettings;
      return;
    }

    this._pendingApnSettings = aNewApnSettings;
    this.deactivateDataCallsAndWait().then(() => {
      this._setupApnSettings(this._pendingApnSettings);
      this._pendingApnSettings = null;
      this.updateRILNetworkInterface();
    });
  },

  updateRILNetworkInterface: function() {
    let networkInterface = this.dataNetworkInterfaces.get(NETWORK_TYPE_MOBILE);
    if (!networkInterface) {
      if (DEBUG) {
        this.debug("No network interface for default data.");
      }
      return;
    }

    let connection =
      gMobileConnectionService.getItemByServiceId(this.clientId);

    // This check avoids data call connection if the radio is not ready
    // yet after toggling off airplane mode.
    let radioState = connection && connection.radioState;
    if (radioState != Ci.nsIMobileConnection.MOBILE_RADIO_STATE_ENABLED) {
      if (DEBUG) {
        this.debug("RIL is not ready for data connection: radio's not ready");
      }
      return;
    }

    // We only watch at "ril.data.enabled" flag changes for connecting or
    // disconnecting the data call. If the value of "ril.data.enabled" is
    // true and any of the remaining flags change the setting application
    // should turn this flag to false and then to true in order to reload
    // the new values and reconnect the data call.
    if (this.dataCallSettings.oldEnabled === this.dataCallSettings.enabled) {
      if (DEBUG) {
        this.debug("No changes for ril.data.enabled flag. Nothing to do.");
      }
      return;
    }

    let dataInfo = connection && connection.data;
    let isRegistered =
      dataInfo &&
      dataInfo.state == RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED;
    let haveDataConnection =
      dataInfo &&
      dataInfo.type != RIL.GECKO_MOBILE_CONNECTION_STATE_UNKNOWN;
    if (!isRegistered || !haveDataConnection) {
      if (DEBUG) {
        this.debug("RIL is not ready for data connection: Phone's not " +
                   "registered or doesn't have data connection.");
      }
      return;
    }
    let wifi_active = false;
    if (gNetworkManager.activeNetworkInfo &&
        gNetworkManager.activeNetworkInfo.type == NETWORK_TYPE_WIFI) {
      wifi_active = true;
    }

    let defaultDataCallConnected = networkInterface.connected;

    // We have moved part of the decision making into DataCall, the rest will be
    // moved after Bug 904514 - [meta] NetworkManager enhancement.
    if (networkInterface.enabled &&
        (!this.dataCallSettings.enabled ||
         (dataInfo.roaming && !this.dataCallSettings.roamingEnabled))) {
      if (DEBUG) {
        this.debug("Data call settings: disconnect data call.");
      }
      networkInterface.disconnect();
      return;
    }

    if (networkInterface.enabled && wifi_active) {
      if (DEBUG) {
        this.debug("Disconnect data call when Wifi is connected.");
      }
      networkInterface.disconnect();
      return;
    }

    if (!this.dataCallSettings.enabled || defaultDataCallConnected) {
      if (DEBUG) {
        this.debug("Data call settings: nothing to do.");
      }
      return;
    }
    if (dataInfo.roaming && !this.dataCallSettings.roamingEnabled) {
      if (DEBUG) {
        this.debug("We're roaming, but data roaming is disabled.");
      }
      return;
    }
    if (wifi_active) {
      if (DEBUG) {
        this.debug("Don't connect data call when Wifi is connected.");
      }
      return;
    }
    if (this._pendingApnSettings) {
      if (DEBUG) this.debug("We're changing apn settings, ignore any changes.");
      return;
    }

    if (this._deactivatingDataCalls) {
      if (DEBUG) this.debug("We're deactivating all data calls, ignore any changes.");
      return;
    }

    if (DEBUG) {
      this.debug("Data call settings: connect data call.");
    }
    networkInterface.connect();
  },

  _isMobileNetworkType: function(aNetworkType) {
    if (aNetworkType === NETWORK_TYPE_MOBILE ||
        aNetworkType === NETWORK_TYPE_MOBILE_MMS ||
        aNetworkType === NETWORK_TYPE_MOBILE_SUPL ||
        aNetworkType === NETWORK_TYPE_MOBILE_IMS ||
        aNetworkType === NETWORK_TYPE_MOBILE_DUN ||
        aNetworkType === NETWORK_TYPE_MOBILE_FOTA) {
      return true;
    }

    return false;
  },

  getDataCallStateByType: function(aNetworkType) {
    if (!this._isMobileNetworkType(aNetworkType)) {
      if (DEBUG) this.debug(aNetworkType + " is not a mobile network type!");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let networkInterface = this.dataNetworkInterfaces.get(aNetworkType);
    if (!networkInterface) {
      return NETWORK_STATE_UNKNOWN;
    }
    return networkInterface.info.state;
  },

  setupDataCallByType: function(aNetworkType) {
    if (DEBUG) {
      this.debug("setupDataCallByType: " + aNetworkType);
    }

    if (!this._isMobileNetworkType(aNetworkType)) {
      if (DEBUG) this.debug(aNetworkType + " is not a mobile network type!");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let networkInterface = this.dataNetworkInterfaces.get(aNetworkType);
    if (!networkInterface) {
      if (DEBUG) {
        this.debug("No network interface for type: " + aNetworkType);
      }
      return;
    }

    networkInterface.connect();
  },

  deactivateDataCallByType: function(aNetworkType) {
    if (DEBUG) {
      this.debug("deactivateDataCallByType: " + aNetworkType);
    }

    if (!this._isMobileNetworkType(aNetworkType)) {
      if (DEBUG) this.debug(aNetworkType + " is not a mobile network type!");
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    let networkInterface = this.dataNetworkInterfaces.get(aNetworkType);
    if (!networkInterface) {
      if (DEBUG) {
        this.debug("No network interface for type: " + aNetworkType);
      }
      return;
    }

    networkInterface.disconnect();
  },

  _deactivatingDataCalls: false,

  deactivateDataCalls: function(aCallback) {
    let dataDisconnecting = false;
    this.dataNetworkInterfaces.forEach(function(networkInterface) {
      if (networkInterface.enabled) {
        if (networkInterface.info.state != NETWORK_STATE_UNKNOWN &&
            networkInterface.info.state != NETWORK_STATE_DISCONNECTED) {
          dataDisconnecting = true;
        }
        networkInterface.disconnect();
      }
    });

    this._deactivatingDataCalls = dataDisconnecting;
    if (!dataDisconnecting) {
      aCallback.notifyDataCallsDisconnected();
      return;
    }

    let callback = {
      notifyAllDataDisconnected: () => {
        this._unregisterListener(callback);
        aCallback.notifyDataCallsDisconnected();
      }
    };
    this._registerListener(callback);
  },

  _listeners: null,

  _notifyListeners: function(aMethodName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (this._listeners.indexOf(listener) == -1) {
        // Listener has been unregistered in previous run.
        continue;
      }

      let handler = listener[aMethodName];
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        this.debug("listener for " + aMethodName + " threw an exception: " + e);
      }
    }
  },

  _registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      return;
    }

    this._listeners.push(aListener);
  },

  _unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },

  _findDataCallByCid: function(aCid) {
    if (aCid === undefined || aCid < 0) {
      return -1;
    }

    for (let i = 0; i < this._dataCalls.length; i++) {
      let datacall = this._dataCalls[i];
      if (datacall.linkInfo.cid != null &&
          datacall.linkInfo.cid == aCid) {
        return i;
      }
    }

    return -1;
  },

  /**
   * Notify about data call setup error, called from DataCall.
   */
  notifyDataCallError: function(aDataCall, aErrorMsg) {
    // Notify data call error only for data APN
    let networkInterface = this.dataNetworkInterfaces.get(NETWORK_TYPE_MOBILE);
    if (networkInterface && networkInterface.enabled) {
      let dataCall = networkInterface.dataCall;
      if (this._compareDataCallOptions(dataCall, aDataCall)) {
        Services.obs.notifyObservers(networkInterface.info,
                                     TOPIC_DATA_CALL_ERROR, aErrorMsg);
      }
    }
  },

  /**
   * Notify about data call changed, called from DataCall.
   */
  notifyDataCallChanged: function(aUpdatedDataCall) {
    // Process pending radio power off request after all data calls
    // are disconnected.
    if (aUpdatedDataCall.state == NETWORK_STATE_DISCONNECTED ||
        aUpdatedDataCall.state == NETWORK_STATE_UNKNOWN &&
        this.allDataDisconnected() && this._deactivatingDataCalls) {
      this._deactivatingDataCalls = false;
      this._notifyListeners("notifyAllDataDisconnected", {
        clientId: this.clientId
      });
    }
  },

  // nsIDataCallInterfaceListener

  notifyDataCallListChanged: function(aCount, aDataCallList) {
    let currentDataCalls = this._dataCalls.slice();
    for (let i = 0; i < aDataCallList.length; i++) {
      let dataCall = aDataCallList[i];
      let index = this._findDataCallByCid(dataCall.cid);
      if (index == -1) {
        if (DEBUG) {
          this.debug("Unexpected new data call: " + JSON.stringify(dataCall));
        }
        continue;
      }
      currentDataCalls[index].onDataCallChanged(dataCall);
      currentDataCalls[index] = null;
    }

    // If there is any CONNECTED DataCall left in currentDataCalls, means that
    // it is missing in dataCallList, we should send a DISCONNECTED event to
    // notify about this.
    for (let i = 0; i < currentDataCalls.length; i++) {
      let currentDataCall = currentDataCalls[i];
      if (currentDataCall && currentDataCall.linkInfo.cid != null &&
          currentDataCall.state == NETWORK_STATE_CONNECTED) {
        if (DEBUG) {
          this.debug("Expected data call missing: " + JSON.stringify(
            currentDataCall.apnProfile) + ", must have been DISCONNECTED.");
        }
        currentDataCall.onDataCallChanged({
          state: NETWORK_STATE_DISCONNECTED
        });
      }
    }
  },

  // nsIMobileConnectionListener

  notifyVoiceChanged: function() {},

  notifyDataChanged: function () {
    let connection = gMobileConnectionService.getItemByServiceId(this.clientId);
    let newDataInfo = connection.data;

    if (this._dataInfo.state == newDataInfo.state &&
        this._dataInfo.type == newDataInfo.type &&
        this._dataInfo.roaming == newDataInfo.roaming) {
      return;
    }

    this._dataInfo.state = newDataInfo.state;
    this._dataInfo.type = newDataInfo.type;
    this._dataInfo.roaming = newDataInfo.roaming;
    this.updateRILNetworkInterface();
  },

  notifyDataError: function (aMessage) {},

  notifyCFStateChanged: function(aAction, aReason, aNumber, aTimeSeconds, aServiceClass) {},

  notifyEmergencyCbModeChanged: function(aActive, aTimeoutMs) {},

  notifyOtaStatusChanged: function(aStatus) {},

  notifyRadioStateChanged: function() {},

  notifyClirModeChanged: function(aMode) {},

  notifyLastKnownNetworkChanged: function() {},

  notifyLastKnownHomeNetworkChanged: function() {},

  notifyNetworkSelectionModeChanged: function() {}
};

function DataCall(aClientId, aApnSetting, aDataCallHandler) {
  this.clientId = aClientId;
  this.dataCallHandler = aDataCallHandler;
  this.apnProfile = {
    apn: aApnSetting.apn,
    user: aApnSetting.user,
    password: aApnSetting.password,
    authType: aApnSetting.authtype,
    protocol: aApnSetting.protocol,
    roaming_protocol: aApnSetting.roaming_protocol
  };
  this.linkInfo = {
    cid: null,
    ifname: null,
    addresses: [],
    dnses: [],
    gateways: [],
    pcscf: [],
    mtu: null
  };
  this.state = NETWORK_STATE_UNKNOWN;
  this.requestedNetworkIfaces = [];
}
DataCall.prototype = {
  /**
   * Standard values for the APN connection retry process
   * Retry funcion: time(secs) = A * numer_of_retries^2 + B
   */
  NETWORK_APNRETRY_FACTOR: 8,
  NETWORK_APNRETRY_ORIGIN: 3,
  NETWORK_APNRETRY_MAXRETRIES: 10,

  dataCallHandler: null,

  // Event timer for connection retries
  timer: null,

  // APN failed connections. Retry counter
  apnRetryCounter: 0,

  // Array to hold RILNetworkInterfaces that requested this DataCall.
  requestedNetworkIfaces: null,

  /**
   * @return "deactivate" if <ifname> changes or one of the aCurrentDataCall
   *         addresses is missing in updatedDataCall, or "identical" if no
   *         changes found, or "changed" otherwise.
   */
  _compareDataCallLink: function(aUpdatedDataCall, aCurrentDataCall) {
    // If network interface is changed, report as "deactivate".
    if (aUpdatedDataCall.ifname != aCurrentDataCall.ifname) {
      return "deactivate";
    }

    // If any existing address is missing, report as "deactivate".
    for (let i = 0; i < aCurrentDataCall.addresses.length; i++) {
      let address = aCurrentDataCall.addresses[i];
      if (aUpdatedDataCall.addresses.indexOf(address) < 0) {
        return "deactivate";
      }
    }

    if (aCurrentDataCall.addresses.length != aUpdatedDataCall.addresses.length) {
      // Since now all |aCurrentDataCall.addresses| are found in
      // |aUpdatedDataCall.addresses|, this means one or more new addresses are
      // reported.
      return "changed";
    }

    let fields = ["gateways", "dnses"];
    for (let i = 0; i < fields.length; i++) {
      // Compare <datacall>.<field>.
      let field = fields[i];
      let lhs = aUpdatedDataCall[field], rhs = aCurrentDataCall[field];
      if (lhs.length != rhs.length) {
        return "changed";
      }
      for (let i = 0; i < lhs.length; i++) {
        if (lhs[i] != rhs[i]) {
          return "changed";
        }
      }
    }

    if (aCurrentDataCall.mtu != aUpdatedDataCall.mtu) {
      return "changed";
    }

    return "identical";
  },

  _getGeckoDataCallState:function (aDataCall) {
    if (aDataCall.active == Ci.nsIDataCallInterface.DATACALL_STATE_ACTIVE_UP ||
        aDataCall.active == Ci.nsIDataCallInterface.DATACALL_STATE_ACTIVE_DOWN) {
      return NETWORK_STATE_CONNECTED;
    }

    return NETWORK_STATE_DISCONNECTED;
  },

  onSetupDataCallResult: function(aDataCall) {
    this.debug("onSetupDataCallResult: " + JSON.stringify(aDataCall));
    let errorMsg = aDataCall.errorMsg;
    if (aDataCall.failCause &&
        aDataCall.failCause != Ci.nsIDataCallInterface.DATACALL_FAIL_NONE) {
      errorMsg =
        RIL.RIL_DATACALL_FAILCAUSE_TO_GECKO_DATACALL_ERROR[aDataCall.failCause];
    }

    if (errorMsg) {
      if (DEBUG) {
        this.debug("SetupDataCall error for apn " + this.apnProfile.apn + ": " +
                   errorMsg + " (" + aDataCall.failCause + "), retry time: " +
                   aDataCall.suggestedRetryTime);
      }

      this.state = NETWORK_STATE_DISCONNECTED;

      if (this.requestedNetworkIfaces.length === 0) {
        if (DEBUG) this.debug("This DataCall is not requested anymore.");
        return;
      }

      // Let DataCallHandler notify MobileConnectionService
      this.dataCallHandler.notifyDataCallError(this, errorMsg);

      // For suggestedRetryTime, the value of INT32_MAX(0x7fffffff) means no retry.
      if (aDataCall.suggestedRetryTime === INT32_MAX ||
          this.isPermanentFail(aDataCall.failCause, errorMsg)) {
        if (DEBUG) this.debug("Data call error: no retry needed.");
        return;
      }

      this.retry(aDataCall.suggestedRetryTime);
      return;
    }

    this.apnRetryCounter = 0;
    this.linkInfo.cid = aDataCall.cid;

    if (this.requestedNetworkIfaces.length === 0) {
      if (DEBUG) {
        this.debug("State is connected, but no network interface requested" +
                   " this DataCall");
      }
      this.deactivate();
      return;
    }

    this.linkInfo.ifname = aDataCall.ifname;
    this.linkInfo.addresses = aDataCall.addresses ? aDataCall.addresses.split(" ") : [];
    this.linkInfo.gateways = aDataCall.gateways ? aDataCall.gateways.split(" ") : [];
    this.linkInfo.dnses = aDataCall.dnses ? aDataCall.dnses.split(" ") : [];
    this.linkInfo.pcscf = aDataCall.pcscf ? aDataCall.pcscf.split(" ") : [];
    this.linkInfo.mtu = aDataCall.mtu > 0 ? aDataCall.mtu : 0;
    this.state = this._getGeckoDataCallState(aDataCall);

    // Notify DataCallHandler about data call connected.
    this.dataCallHandler.notifyDataCallChanged(this);

    for (let i = 0; i < this.requestedNetworkIfaces.length; i++) {
      this.requestedNetworkIfaces[i].notifyRILNetworkInterface();
    }
  },

  onDeactivateDataCallResult: function() {
    if (DEBUG) this.debug("onDeactivateDataCallResult");

    this.reset();

    if (this.requestedNetworkIfaces.length > 0) {
      if (DEBUG) {
        this.debug("State is disconnected/unknown, but this DataCall is" +
                   " requested.");
      }
      this.setup();
      return;
    }

    // Notify DataCallHandler about data call disconnected.
    this.dataCallHandler.notifyDataCallChanged(this);
  },

  onDataCallChanged: function(aUpdatedDataCall) {
    if (DEBUG) {
      this.debug("onDataCallChanged: " + JSON.stringify(aUpdatedDataCall));
    }

    if (this.state == NETWORK_STATE_CONNECTING ||
        this.state == NETWORK_STATE_DISCONNECTING) {
      if (DEBUG) {
        this.debug("We are in connecting/disconnecting state, ignore any " +
                   "unsolicited event for now.");
      }
      return;
    }

    let dataCallState = this._getGeckoDataCallState(aUpdatedDataCall);
    if (this.state == dataCallState &&
        dataCallState != NETWORK_STATE_CONNECTED) {
      return;
    }

    let newLinkInfo = {
      ifname: aUpdatedDataCall.ifname,
      addresses: aUpdatedDataCall.addresses ? aUpdatedDataCall.addresses.split(" ") : [],
      dnses: aUpdatedDataCall.dnses ? aUpdatedDataCall.dnses.split(" ") : [],
      gateways: aUpdatedDataCall.gateways ? aUpdatedDataCall.gateways.split(" ") : [],
      pcscf: aUpdatedDataCall.pcscf ? aUpdatedDataCall.pcscf.split(" ") : [],
      mtu: aUpdatedDataCall.mtu > 0 ? aUpdatedDataCall.mtu : 0
    };

    switch (dataCallState) {
      case NETWORK_STATE_CONNECTED:
        if (this.state == NETWORK_STATE_CONNECTED) {
          let result =
            this._compareDataCallLink(newLinkInfo, this.linkInfo);

          if (result == "identical") {
            if (DEBUG) this.debug("No changes in data call.");
            return;
          }
          if (result == "deactivate") {
            if (DEBUG) this.debug("Data link changed, cleanup.");
            this.deactivate();
            return;
          }
          // Minor change, just update and notify.
          if (DEBUG) {
            this.debug("Data link minor change, just update and notify.");
          }

          this.linkInfo.addresses = newLinkInfo.addresses.slice();
          this.linkInfo.gateways = newLinkInfo.gateways.slice();
          this.linkInfo.dnses = newLinkInfo.dnses.slice();
          this.linkInfo.pcscf = newLinkInfo.pcscf.slice();
          this.linkInfo.mtu = newLinkInfo.mtu;
        }
        break;
      case NETWORK_STATE_DISCONNECTED:
      case NETWORK_STATE_UNKNOWN:
        if (this.state == NETWORK_STATE_CONNECTED) {
          // Notify first on unexpected data call disconnection.
          this.state = dataCallState;
          for (let i = 0; i < this.requestedNetworkIfaces.length; i++) {
            this.requestedNetworkIfaces[i].notifyRILNetworkInterface();
          }
        }
        this.reset();

        if (this.requestedNetworkIfaces.length > 0) {
          if (DEBUG) {
            this.debug("State is disconnected/unknown, but this DataCall is" +
                       " requested.");
          }
          this.setup();
          return;
        }
        break;
    }

    this.state = dataCallState;

    // Notify DataCallHandler about data call changed.
    this.dataCallHandler.notifyDataCallChanged(this);

    for (let i = 0; i < this.requestedNetworkIfaces.length; i++) {
      this.requestedNetworkIfaces[i].notifyRILNetworkInterface();
    }
  },

  // Helpers

  debug: function(aMsg) {
    dump("-*- DataCall[" + this.clientId + ":" + this.apnProfile.apn + "]: " +
      aMsg + "\n");
  },

  get connected() {
    return this.state == NETWORK_STATE_CONNECTED;
  },

  isPermanentFail: function(aDataFailCause, aErrorMsg) {
    // Check ril.h for 'no retry' data call fail causes.
    if (aErrorMsg === RIL.GECKO_ERROR_RADIO_NOT_AVAILABLE ||
        aErrorMsg === RIL.GECKO_ERROR_INVALID_PARAMETER ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_OPERATOR_BARRED ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_MISSING_UKNOWN_APN ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_UNKNOWN_PDP_ADDRESS_TYPE ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_USER_AUTHENTICATION ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_ACTIVATION_REJECT_GGSN ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_SERVICE_OPTION_NOT_SUPPORTED ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_NSAPI_IN_USE ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_ONLY_IPV4_ALLOWED ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_ONLY_IPV6_ALLOWED ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_PROTOCOL_ERRORS ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_RADIO_POWER_OFF ||
        aDataFailCause === Ci.nsIDataCallInterface.DATACALL_FAIL_TETHERED_CALL_ACTIVE) {
      return true;
    }

    return false;
  },

  inRequestedTypes: function(aType) {
    for (let i = 0; i < this.requestedNetworkIfaces.length; i++) {
      if (this.requestedNetworkIfaces[i].info.type == aType) {
        return true;
      }
    }
    return false;
  },

  canHandleApn: function(aApnSetting) {
    let isIdentical = this.apnProfile.apn == aApnSetting.apn &&
                      (this.apnProfile.user || '') == (aApnSetting.user || '') &&
                      (this.apnProfile.password || '') == (aApnSetting.password || '') &&
                      (this.apnProfile.authType || '') == (aApnSetting.authtype || '');

    if (RILQUIRKS_HAVE_IPV6) {
      isIdentical = isIdentical &&
                    (this.apnProfile.protocol || '') == (aApnSetting.protocol || '') &&
                    (this.apnProfile.roaming_protocol || '') == (aApnSetting.roaming_protocol || '');
    }

    return isIdentical;
  },

  resetLinkInfo: function() {
    this.linkInfo.cid = null;
    this.linkInfo.ifname = null;
    this.linkInfo.addresses = [];
    this.linkInfo.dnses = [];
    this.linkInfo.gateways = [];
    this.linkInfo.pcscf = [];
    this.linkInfo.mtu = null;
  },

  reset: function() {
    this.resetLinkInfo();

    this.state = NETWORK_STATE_UNKNOWN;
  },

  connect: function(aNetworkInterface) {
    if (DEBUG) this.debug("connect: " + aNetworkInterface.info.type);

    if (this.requestedNetworkIfaces.indexOf(aNetworkInterface) == -1) {
      this.requestedNetworkIfaces.push(aNetworkInterface);
    }

    if (this.state == NETWORK_STATE_CONNECTING ||
        this.state == NETWORK_STATE_DISCONNECTING) {
      return;
    }
    if (this.state == NETWORK_STATE_CONNECTED) {
      // This needs to run asynchronously, to behave the same way as the case of
      // non-shared apn, see bug 1059110.
      Services.tm.currentThread.dispatch(() => {
        // Do not notify if state changed while this event was being dispatched,
        // the state probably was notified already or need not to be notified.
        if (aNetworkInterface.info.state == RIL.GECKO_NETWORK_STATE_CONNECTED) {
          aNetworkInterface.notifyRILNetworkInterface();
        }
      }, Ci.nsIEventTarget.DISPATCH_NORMAL);
      return;
    }

    // If retry mechanism is running on background, stop it since we are going
    // to setup data call now.
    if (this.timer) {
      this.timer.cancel();
    }

    this.setup();
  },

  setup: function() {
    if (DEBUG) {
      this.debug("Going to set up data connection with APN " +
                 this.apnProfile.apn);
    }

    let connection =
      gMobileConnectionService.getItemByServiceId(this.clientId);
    let dataInfo = connection && connection.data;
    if (dataInfo == null ||
        dataInfo.state != RIL.GECKO_MOBILE_CONNECTION_STATE_REGISTERED ||
        dataInfo.type == RIL.GECKO_MOBILE_CONNECTION_STATE_UNKNOWN) {
      return;
    }

    let radioTechType = dataInfo.type;
    let radioTechnology = RIL.GECKO_RADIO_TECH.indexOf(radioTechType);
    let authType = RIL.RIL_DATACALL_AUTH_TO_GECKO.indexOf(this.apnProfile.authType);
    // Use the default authType if the value in database is invalid.
    // For the case that user might not select the authentication type.
    if (authType == -1) {
      if (DEBUG) {
        this.debug("Invalid authType '" + this.apnProfile.authtype +
                   "', using '" + RIL.GECKO_DATACALL_AUTH_DEFAULT + "'");
      }
      authType = RIL.RIL_DATACALL_AUTH_TO_GECKO.indexOf(RIL.GECKO_DATACALL_AUTH_DEFAULT);
    }

    let pdpType = Ci.nsIDataCallInterface.DATACALL_PDP_TYPE_IPV4;
    if (RILQUIRKS_HAVE_IPV6) {
      pdpType = !dataInfo.roaming
              ? RIL.RIL_DATACALL_PDP_TYPES.indexOf(this.apnProfile.protocol)
              : RIL.RIL_DATACALL_PDP_TYPES.indexOf(this.apnProfile.roaming_protocol);
      if (pdpType == -1) {
        if (DEBUG) {
          this.debug("Invalid pdpType '" + (!dataInfo.roaming
                     ? this.apnProfile.protocol
                     : this.apnProfile.roaming_protocol) +
                     "', using '" + RIL.GECKO_DATACALL_PDP_TYPE_DEFAULT + "'");
        }
        pdpType = RIL.RIL_DATACALL_PDP_TYPES.indexOf(RIL.GECKO_DATACALL_PDP_TYPE_DEFAULT);
      }
    }

    let dcInterface = this.dataCallHandler.dataCallInterface;
    dcInterface.setupDataCall(
      this.apnProfile.apn, this.apnProfile.user, this.apnProfile.password,
      authType, pdpType, {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallCallback]),
        notifySetupDataCallSuccess: (aDataCall) => {
          this.onSetupDataCallResult(aDataCall);
        },
        notifyError: (aErrorMsg) => {
          this.onSetupDataCallResult({errorMsg: aErrorMsg});
        }
      });
    this.state = NETWORK_STATE_CONNECTING;
  },

  retry: function(aSuggestedRetryTime) {
    let apnRetryTimer;

    // We will retry the connection in increasing times
    // based on the function: time = A * numer_of_retries^2 + B
    if (this.apnRetryCounter >= this.NETWORK_APNRETRY_MAXRETRIES) {
      this.apnRetryCounter = 0;
      this.timer = null;
      if (DEBUG) this.debug("Too many APN Connection retries - STOP retrying");
      return;
    }

    // If there is a valid aSuggestedRetryTime, override the retry timer.
    if (aSuggestedRetryTime !== undefined && aSuggestedRetryTime >= 0) {
      apnRetryTimer = aSuggestedRetryTime / 1000;
    } else {
      apnRetryTimer = this.NETWORK_APNRETRY_FACTOR *
                      (this.apnRetryCounter * this.apnRetryCounter) +
                      this.NETWORK_APNRETRY_ORIGIN;
    }
    this.apnRetryCounter++;
    if (DEBUG) {
      this.debug("Data call - APN Connection Retry Timer (secs-counter): " +
                 apnRetryTimer + "-" + this.apnRetryCounter);
    }

    if (this.timer == null) {
      // Event timer for connection retries
      this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    this.timer.initWithCallback(this, apnRetryTimer * 1000,
                                Ci.nsITimer.TYPE_ONE_SHOT);
  },

  disconnect: function(aNetworkInterface) {
    if (DEBUG) this.debug("disconnect: " + aNetworkInterface.info.type);

    let index = this.requestedNetworkIfaces.indexOf(aNetworkInterface);
    if (index != -1) {
      this.requestedNetworkIfaces.splice(index, 1);

      if (this.state == NETWORK_STATE_DISCONNECTED ||
          this.state == NETWORK_STATE_UNKNOWN) {
        if (this.timer) {
          this.timer.cancel();
        }
        this.reset();
        return;
      }

      // Notify the DISCONNECTED event immediately after network interface is
      // removed from requestedNetworkIfaces, to make the DataCall, shared or
      // not, to have the same behavior.
      Services.tm.currentThread.dispatch(() => {
        // Do not notify if state changed while this event was being dispatched,
        // the state probably was notified already or need not to be notified.
        if (aNetworkInterface.info.state == RIL.GECKO_NETWORK_STATE_DISCONNECTED) {
          aNetworkInterface.notifyRILNetworkInterface();

          // Clear link info after notifying NetworkManager.
          if (this.requestedNetworkIfaces.length === 0) {
            this.resetLinkInfo();
          }
        }
      }, Ci.nsIEventTarget.DISPATCH_NORMAL);
    }

    // Only deactivate data call if no more network interface needs this
    // DataCall and if state is CONNECTED, for other states, we simply remove
    // the network interface from requestedNetworkIfaces.
    if (this.requestedNetworkIfaces.length > 0 ||
        this.state != NETWORK_STATE_CONNECTED) {
      return;
    }

    this.deactivate();
  },

  deactivate: function() {
    let reason = Ci.nsIDataCallInterface.DATACALL_DEACTIVATE_NO_REASON;
    if (DEBUG) {
      this.debug("Going to disconnect data connection cid " + this.linkInfo.cid);
    }

    let dcInterface = this.dataCallHandler.dataCallInterface;
    dcInterface.deactivateDataCall(this.linkInfo.cid, reason, {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallCallback]),
      notifySuccess: () => {
        this.onDeactivateDataCallResult();
      },
      notifyError: (aErrorMsg) => {
        this.onDeactivateDataCallResult();
      }
    });

    this.state = NETWORK_STATE_DISCONNECTING;
  },

  // Entry method for timer events. Used to reconnect to a failed APN
  notify: function(aTimer) {
    this.setup();
  },

  shutdown: function() {
    if (this.timer) {
      this.timer.cancel();
      this.timer = null;
    }
  }
};

function RILNetworkInfo(aClientId, aType, aNetworkInterface)
{
  this.serviceId = aClientId;
  this.type = aType;

  this.networkInterface = aNetworkInterface;
}
RILNetworkInfo.prototype = {
  classID:   RILNETWORKINFO_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILNETWORKINFO_CID,
                                    classDescription: "RILNetworkInfo",
                                    interfaces: [Ci.nsINetworkInfo,
                                                 Ci.nsIRilNetworkInfo]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInfo,
                                         Ci.nsIRilNetworkInfo]),

  networkInterface: null,

  getDataCall: function() {
    return this.networkInterface.dataCall;
  },

  getApnSetting: function() {
    return this.networkInterface.apnSetting;
  },

  debug: function(aMsg) {
    dump("-*- RILNetworkInfo[" + this.serviceId + ":" + this.type + "]: " +
         aMsg + "\n");
  },

  /**
   * nsINetworkInfo Implementation
   */
  get state() {
    let dataCall = this.getDataCall();
    if (!dataCall.inRequestedTypes(this.type)) {
      return NETWORK_STATE_DISCONNECTED;
    }
    return dataCall.state;
  },

  type: null,

  get name() {
    return this.getDataCall().linkInfo.ifname;
  },

  getAddresses: function(aIps, aPrefixLengths) {
    let addresses = this.getDataCall().linkInfo.addresses;

    let ips = [];
    let prefixLengths = [];
    for (let i = 0; i < addresses.length; i++) {
      let [ip, prefixLength] = addresses[i].split("/");
      ips.push(ip);
      prefixLengths.push(prefixLength);
    }

    aIps.value = ips.slice();
    aPrefixLengths.value = prefixLengths.slice();

    return ips.length;
  },

  getGateways: function(aCount) {
    let linkInfo = this.getDataCall().linkInfo;

    if (aCount) {
      aCount.value = linkInfo.gateways.length;
    }

    return linkInfo.gateways.slice();
  },

  getDnses: function(aCount) {
    let linkInfo = this.getDataCall().linkInfo;

    if (aCount) {
      aCount.value = linkInfo.dnses.length;
    }

    return linkInfo.dnses.slice();
  },

  /**
   * nsIRilNetworkInfo Implementation
   */

  serviceId: 0,

  get iccId() {
    let icc = gIccService.getIccByServiceId(this.serviceId);
    let iccInfo = icc && icc.iccInfo;

    return iccInfo && iccInfo.iccid;
  },

  get mmsc() {
    if (this.type != NETWORK_TYPE_MOBILE_MMS) {
      if (DEBUG) this.debug("Error! Only MMS network can get MMSC.");
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return this.getApnSetting().mmsc || "";
  },

  get mmsProxy() {
    if (this.type != NETWORK_TYPE_MOBILE_MMS) {
      if (DEBUG) this.debug("Error! Only MMS network can get MMS proxy.");
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return this.getApnSetting().mmsproxy || "";
  },

  get mmsPort() {
    if (this.type != NETWORK_TYPE_MOBILE_MMS) {
      if (DEBUG) this.debug("Error! Only MMS network can get MMS port.");
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    // Note: Port 0 is reserved, so we treat it as invalid as well.
    // See http://www.iana.org/assignments/port-numbers
    return this.getApnSetting().mmsport || -1;
  },

  getPcscf: function(aCount) {
    if (this.type != NETWORK_TYPE_MOBILE_IMS) {
      if (DEBUG) this.debug("Error! Only IMS network can get pcscf.");
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    let linkInfo = this.getDataCall().linkInfo;

    if (aCount) {
      aCount.value = linkInfo.pcscf.length;
    }
    return linkInfo.pcscf.slice();
  },
};

function RILNetworkInterface(aDataCallHandler, aType, aApnSetting, aDataCall) {
  if (!aDataCall) {
    throw new Error("No dataCall for RILNetworkInterface: " + type);
  }

  this.dataCallHandler = aDataCallHandler;
  this.enabled = false;
  this.dataCall = aDataCall;
  this.apnSetting = aApnSetting;

  this.info = new RILNetworkInfo(aDataCallHandler.clientId, aType, this);
}

RILNetworkInterface.prototype = {
  classID:   RILNETWORKINTERFACE_CID,
  classInfo: XPCOMUtils.generateCI({classID: RILNETWORKINTERFACE_CID,
                                    classDescription: "RILNetworkInterface",
                                    interfaces: [Ci.nsINetworkInterface]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINetworkInterface]),

  // If this RILNetworkInterface type is enabled or not.
  enabled: null,

  apnSetting: null,

  dataCall: null,

  /**
   * nsINetworkInterface Implementation
   */

  info: null,

  get httpProxyHost() {
    return this.apnSetting.proxy || "";
  },

  get httpProxyPort() {
    return this.apnSetting.port || "";
  },

  get mtu() {
    // Value provided by network has higher priority than apn settings.
    return this.dataCall.linkInfo.mtu || this.apnSetting.mtu || -1;
  },

  // Helpers

  debug: function(aMsg) {
    dump("-*- RILNetworkInterface[" + this.dataCallHandler.clientId + ":" +
         this.info.type + "]: " + aMsg + "\n");
  },

  get connected() {
    return this.info.state == NETWORK_STATE_CONNECTED;
  },

  notifyRILNetworkInterface: function() {
    if (DEBUG) {
      this.debug("notifyRILNetworkInterface type: " + this.info.type +
                 ", state: " + this.info.state);
    }

    gNetworkManager.updateNetworkInterface(this);
  },

  connect: function() {
    this.enabled = true;

    this.dataCall.connect(this);
  },

  disconnect: function() {
    if (!this.enabled) {
      return;
    }
    this.enabled = false;

    this.dataCall.disconnect(this);
  },

  shutdown: function() {
    this.dataCall.shutdown();
    this.dataCall = null;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataCallManager]);