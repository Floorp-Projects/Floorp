/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const PREF_DEBUG = "dom.payment.debug";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gIccService",
                                   "@mozilla.org/icc/iccservice;1",
                                   "nsIIccService");

XPCOMUtils.defineLazyServiceGetter(this, "gRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

const kMozSettingsChangedObserverTopic = "mozsettings-changed";
const kRilDefaultDataServiceId = "ril.data.defaultServiceId";
const kRilDefaultPaymentServiceId = "ril.payment.defaultServiceId";

let _debug;
try {
  _debug = Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
           && Services.prefs.getBoolPref(PREF_DEBUG);
} catch(e){
  _debug = false;
}

function LOG(s) {
  if (!_debug) {
    return;
  }
  dump("== Payment Provider == " + s + "\n");
}

function LOGE(s) {
  dump("== Payment Provider ERROR == " + s + "\n");
}

function PaymentSettings() {
  Services.obs.addObserver(this, kMozSettingsChangedObserverTopic, false);

  [kRilDefaultDataServiceId, kRilDefaultPaymentServiceId].forEach(setting => {
    gSettingsService.createLock().get(setting, this);
  });
}

PaymentSettings.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISettingsServiceCallback,
                                         Ci.nsIObserver]),

  dataServiceId: 0,
  _paymentServiceId: 0,

  get paymentServiceId() {
    return this._paymentServiceId;
  },

  set paymentServiceId(serviceId) {
    // We allow the payment provider to set the service ID that will be used
    // for the payment process.
    // This service ID will be the one used by the silent SMS flow.
    // If the payment is done with an external SIM, the service ID must be set
    // to null.
    if (serviceId != null && serviceId >= gRil.numRadioInterfaces) {
      LOGE("Invalid service ID " + serviceId);
      return;
    }

    gSettingsService.createLock().set(kRilDefaultPaymentServiceId,
                                      serviceId, 0);
    this._paymentServiceId = serviceId;
  },

  setServiceId: function(aName, aValue) {
    switch (aName) {
      case kRilDefaultDataServiceId:
        this.dataServiceId = aValue;
        if (_debug) {
          LOG("dataServiceId " + this.dataServiceId);
        }
        break;
      case kRilDefaultPaymentServiceId:
        this._paymentServiceId = aValue;
        if (_debug) {
          LOG("paymentServiceId " + this._paymentServiceId);
        }
        break;
    }
  },

  handle: function(aName, aValue) {
    if (aName != kRilDefaultDataServiceId) {
      return;
    }

    this.setServiceId(aName, aValue);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic != kMozSettingsChangedObserverTopic) {
      return;
    }

    try {
      if ("wrappedJSObject" in aSubject) {
        aSubject = aSubject.wrappedJSObject;
      }
      if (!aSubject.key ||
          (aSubject.key !== kRilDefaultDataServiceId &&
           aSubject.key !== kRilDefaultPaymentServiceId)) {
        return;
      }
      this.setServiceId(aSubject.key, aSubject.value);
    } catch (e) {
      LOGE(e);
    }
  },

  cleanup: function() {
    Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
  }
};

function PaymentProviderStrategy() {
  this._settings = new PaymentSettings();
}

PaymentProviderStrategy.prototype = {
  get paymentServiceId() {
    return this._settings.paymentServiceId;
  },

  set paymentServiceId(aServiceId) {
    this._settings.paymentServiceId = aServiceId;
  },

  get iccInfo() {
    if (!this._iccInfo) {
      this._iccInfo = [];
      for (let i = 0; i < gRil.numRadioInterfaces; i++) {
        let icc = gIccService.getIccByServiceId(i);
        let info = icc && icc.iccInfo;
        if (!info) {
          LOGE("Tried to get the ICC info for an invalid service ID " + i);
          continue;
        }

        this._iccInfo.push({
          iccId: info.iccid,
          mcc: info.mcc,
          mnc: info.mnc,
          dataPrimary: i == this._settings.dataServiceId
        });
      }
    }
    return this._iccInfo;
  },

  cleanup: function() {
    this._settings.cleanup();
  },

  classID: Components.ID("{4834b2e1-2c91-44ea-b020-e2581ed279a4}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentProviderStrategy])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentProviderStrategy]);
