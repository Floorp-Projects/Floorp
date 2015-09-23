/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const DATACALLINTERFACE_CONTRACTID = "@mozilla.org/datacall/interface;1";
const DATACALLINTERFACESERVICE_CONTRACTID =
  "@mozilla.org/datacall/interfaceservice;1";
const DATACALLINTERFACE_CID =
  Components.ID("{ff669306-4390-462a-989b-ba37fc42153f}");
const DATACALLINTERFACESERVICE_CID =
  Components.ID("{e23e9337-592d-40b9-8cef-7bd47c28b72e}");

const TOPIC_XPCOM_SHUTDOWN      = "xpcom-shutdown";
const TOPIC_PREF_CHANGED        = "nsPref:changed";
const PREF_RIL_DEBUG_ENABLED    = "ril.debugging.enabled";

XPCOMUtils.defineLazyGetter(this, "RIL", function () {
  let obj = {};
  Cu.import("resource://gre/modules/ril_consts.js", obj);
  return obj;
});

XPCOMUtils.defineLazyServiceGetter(this, "gRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gMobileConnectionService",
                                   "@mozilla.org/mobileconnection/mobileconnectionservice;1",
                                   "nsIMobileConnectionService");

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

function DataCall(aAttributes) {
  for (let key in aAttributes) {
    if (key === "pdpType") {
      // Convert pdp type into constant int value.
      this[key] = RIL.RIL_DATACALL_PDP_TYPES.indexOf(aAttributes[key]);
      continue;
    }

    this[key] = aAttributes[key];
  }
}
DataCall.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCall]),

  failCause: Ci.nsIDataCallInterface.DATACALL_FAIL_NONE,
  suggestedRetryTime: -1,
  cid: -1,
  active: -1,
  pdpType: -1,
  ifname: null,
  addreses: null,
  dnses: null,
  gateways: null,
  pcscf: null,
  mtu: -1
};

function DataCallInterfaceService() {
  this._dataCallInterfaces = [];

  let numClients = gRil.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    this._dataCallInterfaces.push(new DataCallInterface(i));
  }

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN, false);
  Services.prefs.addObserver(PREF_RIL_DEBUG_ENABLED, this, false);
}
DataCallInterfaceService.prototype = {
  classID:   DATACALLINTERFACESERVICE_CID,
  classInfo: XPCOMUtils.generateCI({
    classID: DATACALLINTERFACESERVICE_CID,
    contractID: DATACALLINTERFACESERVICE_CONTRACTID,
    classDescription: "Data Call Interface Service",
    interfaces: [Ci.nsIDataCallInterfaceService,
                 Ci.nsIGonkDataCallInterfaceService]
  }),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallInterfaceService,
                                         Ci.nsIGonkDataCallInterfaceService],
                                         Ci.nsIObserver),

  // An array of DataCallInterface instances.
  _dataCallInterfaces: null,

  debug: function(aMessage) {
    dump("-*- DataCallInterfaceService: " + aMessage + "\n");
  },

  // nsIDataCallInterfaceService

  getDataCallInterface: function(aClientId) {
    let dataCallInterface = this._dataCallInterfaces[aClientId];
    if (!dataCallInterface) {
      throw Cr.NS_ERROR_UNEXPECTED;
    }

    return dataCallInterface;
  },

  // nsIGonkDataCallInterfaceService

  notifyDataCallListChanged: function(aClientId, aCount, aDataCalls) {
    let dataCallInterface = this.getDataCallInterface(aClientId);
    dataCallInterface.handleDataCallListChanged(aCount, aDataCalls);
  },

  // nsIObserver

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.prefs.removeObserver(PREF_RIL_DEBUG_ENABLED, this);
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        break;
    }
  },
};

function DataCallInterface(aClientId) {
  this._clientId = aClientId;
  this._radioInterface = gRil.getRadioInterface(aClientId);
  this._listeners = [];

  if (DEBUG) this.debug("DataCallInterface: " + aClientId);
}
DataCallInterface.prototype = {
  classID:   DATACALLINTERFACE_CID,
  classInfo: XPCOMUtils.generateCI({classID: DATACALLINTERFACE_CID,
                                    contractID: DATACALLINTERFACE_CONTRACTID,
                                    classDescription: "Data Call Interface",
                                    interfaces: [Ci.nsIDataCallInterface]}),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDataCallInterface]),

  debug: function(aMessage) {
    dump("-*- DataCallInterface[" + this._clientId + "]: " + aMessage + "\n");
  },

  _clientId: -1,

  _radioInterface: null,

  _listeners: null,

  // nsIDataCallInterface

  setupDataCall: function(aApn, aUsername, aPassword, aAuthType, aPdpType,
                          aCallback) {
    let connection =
      gMobileConnectionService.getItemByServiceId(this._clientId);
    let dataInfo = connection && connection.data;
    let radioTechType = dataInfo.type;
    let radioTechnology = RIL.GECKO_RADIO_TECH.indexOf(radioTechType);
    // Convert pdp type into string value.
    let pdpType = RIL.RIL_DATACALL_PDP_TYPES[aPdpType];

    this._radioInterface.sendWorkerMessage("setupDataCall", {
      radioTech: radioTechnology,
      apn: aApn,
      user: aUsername,
      passwd: aPassword,
      chappap: aAuthType,
      pdptype: pdpType
    }, (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
      } else {
        let dataCall = new DataCall(aResponse);
        aCallback.notifySetupDataCallSuccess(dataCall);
      }
    });
  },

  deactivateDataCall: function(aCid, aReason, aCallback) {
    this._radioInterface.sendWorkerMessage("deactivateDataCall", {
      cid: aCid,
      reason: aReason
    }, (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
      } else {
        aCallback.notifySuccess();
      }
    });
  },

  getDataCallList: function(aCallback) {
    this._radioInterface.sendWorkerMessage("getDataCallList", null,
      (aResponse) => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          let dataCalls = aResponse.datacalls.map(
            dataCall => new DataCall(dataCall));
          aCallback.notifyGetDataCallListSuccess(dataCalls.length, dataCalls);
        }
      });
  },

  setDataRegistration: function(aAttach, aCallback) {
    this._radioInterface.sendWorkerMessage("setDataRegistration", {
      attach: aAttach
    }, (aResponse) => {
      if (aResponse.errorMsg) {
        aCallback.notifyError(aResponse.errorMsg);
      } else {
        aCallback.notifySuccess();
      }
    });
  },

  handleDataCallListChanged: function(aCount, aDataCalls) {
    this._notifyAllListeners("notifyDataCallListChanged", [aCount, aDataCalls]);
  },

  _notifyAllListeners: function(aMethodName, aArgs) {
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
        if (DEBUG) {
          this.debug("listener for " + aMethodName + " threw an exception: " + e);
        }
      }
    }
  },

  registerListener: function(aListener) {
    if (this._listeners.indexOf(aListener) >= 0) {
      return;
    }

    this._listeners.push(aListener);
  },

  unregisterListener: function(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataCallInterfaceService]);