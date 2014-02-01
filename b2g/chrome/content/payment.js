/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS shim contains the callbacks to fire DOMRequest events for
// navigator.pay API within the payment processor's scope.

"use strict";

let { classes: Cc, interfaces: Ci, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_DEBUG = "dom.payment.debug";

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
  dump("== Payment flow == " + s + "\n");
}

function LOGE(s) {
  dump("== Payment flow ERROR == " + s + "\n");
}

if (_debug) {
  LOG("Frame script injected");
}

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "gRil",
                                   "@mozilla.org/ril;1",
                                   "nsIRadioInterfaceLayer");

XPCOMUtils.defineLazyServiceGetter(this, "iccProvider",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIIccProvider");

XPCOMUtils.defineLazyServiceGetter(this, "smsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

XPCOMUtils.defineLazyServiceGetter(this, "gSettingsService",
                                   "@mozilla.org/settingsService;1",
                                   "nsISettingsService");

const kSilentSmsReceivedTopic = "silent-sms-received";
const kMozSettingsChangedObserverTopic = "mozsettings-changed";

const kRilDefaultDataServiceId = "ril.data.defaultServiceId";
const kRilDefaultPaymentServiceId = "ril.payment.defaultServiceId";

const MOBILEMESSAGECALLBACK_CID =
  Components.ID("{b484d8c9-6be4-4f94-ab60-c9c7ebcc853d}");

// In order to send messages through nsISmsService, we need to implement
// nsIMobileMessageCallback, as the WebSMS API implementation is not usable
// from JS.
function SilentSmsRequest() {
}

SilentSmsRequest.prototype = {
  __exposedProps__: {
    onsuccess: "rw",
    onerror: "rw"
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileMessageCallback]),

  classID: MOBILEMESSAGECALLBACK_CID,

  set onsuccess(aSuccessCallback) {
    this._onsuccess = aSuccessCallback;
  },

  set onerror(aErrorCallback) {
    this._onerror = aErrorCallback;
  },

  notifyMessageSent: function notifyMessageSent(aMessage) {
    if (_debug) {
      LOG("Silent message successfully sent");
    }
    this._onsuccess(aMessage);
  },

  notifySendMessageFailed: function notifySendMessageFailed(aError) {
    LOGE("Error sending silent message " + aError);
    this._onerror(aError);
  }
};

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
                                      serviceId, null);
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
      let setting = JSON.parse(aData);
      if (!setting.key ||
          (setting.key !== kRilDefaultDataServiceId &&
           setting.key !== kRilDefaultPaymentServiceId)) {
        return;
      }
      this.setServiceId(setting.key, setting.value);
    } catch (e) {
      LOGE(e);
    }
  },

  cleanup: function() {
    Services.obs.removeObserver(this, kMozSettingsChangedObserverTopic);
  }
};
#endif

const kClosePaymentFlowEvent = "close-payment-flow-dialog";

let gRequestId;

let gBrowser = Services.wm.getMostRecentWindow("navigator:browser");

let PaymentProvider = {
#ifdef MOZ_B2G_RIL
  __exposedProps__: {
    paymentSuccess: "r",
    paymentFailed: "r",
    paymentServiceId: "rw",
    iccInfo: "r",
    sendSilentSms: "r",
    observeSilentSms: "r",
    removeSilentSmsObserver: "r"
  },
#else
  __exposedProps__: {
    paymentSuccess: "r",
    paymentFailed: "r"
  },
#endif

  _init: function _init() {
#ifdef MOZ_B2G_RIL
    this._settings = new PaymentSettings();
#endif
  },

  _closePaymentFlowDialog: function _closePaymentFlowDialog(aCallback) {
    // After receiving the payment provider confirmation about the
    // successful or failed payment flow, we notify the UI to close the
    // payment flow dialog and return to the caller application.
    let id = kClosePaymentFlowEvent + "-" + uuidgen.generateUUID().toString();

    let content = gBrowser.getContentWindow();
    if (!content) {
      return;
    }

    let detail = {
      type: kClosePaymentFlowEvent,
      id: id,
      requestId: gRequestId
    };

    // In order to avoid race conditions, we wait for the UI to notify that
    // it has successfully closed the payment flow and has recovered the
    // caller app, before notifying the parent process to fire the success
    // or error event over the DOMRequest.
    content.addEventListener("mozContentEvent",
                             function closePaymentFlowReturn(evt) {
      if (evt.detail.id == id && aCallback) {
        aCallback();
      }

      content.removeEventListener("mozContentEvent",
                                  closePaymentFlowReturn);

      let glue = Cc["@mozilla.org/payment/ui-glue;1"]
                   .createInstance(Ci.nsIPaymentUIGlue);
      glue.cleanup();
    });

    gBrowser.shell.sendChromeEvent(detail);

#ifdef MOZ_B2G_RIL
    this._cleanUp();
#endif
  },

  paymentSuccess: function paymentSuccess(aResult) {
    if (_debug) {
      LOG("paymentSuccess " + aResult);
    }

    PaymentProvider._closePaymentFlowDialog(function notifySuccess() {
      if (!gRequestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Success", { result: aResult,
                                                 requestId: gRequestId });
    });
  },

  paymentFailed: function paymentFailed(aErrorMsg) {
    LOGE("paymentFailed " + aErrorMsg);

    PaymentProvider._closePaymentFlowDialog(function notifyError() {
      if (!gRequestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aErrorMsg,
                                                requestId: gRequestId });
    });
  },

#ifdef MOZ_B2G_RIL
  get paymentServiceId() {
    return this._settings.paymentServiceId;
  },

  set paymentServiceId(serviceId) {
    this._settings.paymentServiceId = serviceId;
  },

  // We expose to the payment provider the information of all the SIMs
  // available in the device. iccInfo is an object of this form:
  //  {
  //    "serviceId1": {
  //       mcc: <string>,
  //       mnc: <string>,
  //       iccId: <string>,
  //       dataPrimary: <boolean>
  //     },
  //    "serviceIdN": {...}
  //  }
  get iccInfo() {
    if (!this._iccInfo) {
      this._iccInfo = {};
      for (let i = 0; i < gRil.numRadioInterfaces; i++) {
        let info = iccProvider.getIccInfo(i);
        if (!info) {
          LOGE("Tried to get the ICC info for an invalid service ID " + i);
          continue;
        }

        this._iccInfo[i] = {
          iccId: info.iccid,
          mcc: info.mcc,
          mnc: info.mnc,
          dataPrimary: i == this._settings.dataServiceId
        };
      }
    }

    return Cu.cloneInto(this._iccInfo, content);
  },

  _silentNumbers: null,
  _silentSmsObservers: null,

  sendSilentSms: function sendSilentSms(aNumber, aMessage) {
    if (_debug) {
      LOG("Sending silent message " + aNumber + " - " + aMessage);
    }

    let request = new SilentSmsRequest();

    if (this._settings.paymentServiceId === null) {
      LOGE("No payment service ID set. Cannot send silent SMS");
      let runnable = {
        run: function run() {
          request.notifySendMessageFailed("NO_PAYMENT_SERVICE_ID");
        }
      };
      Services.tm.currentThread.dispatch(runnable,
                                         Ci.nsIThread.DISPATCH_NORMAL);
      return request;
    }

    smsService.send(this._settings.paymentServiceId, aNumber, aMessage, true,
                    request);
    return request;
  },

  observeSilentSms: function observeSilentSms(aNumber, aCallback) {
    if (_debug) {
      LOG("observeSilentSms " + aNumber);
    }

    if (!this._silentSmsObservers) {
      this._silentSmsObservers = {};
      this._silentNumbers = [];
      Services.obs.addObserver(this._onSilentSms.bind(this),
                               kSilentSmsReceivedTopic,
                               false);
    }

    if (!this._silentSmsObservers[aNumber]) {
      this._silentSmsObservers[aNumber] = [];
      this._silentNumbers.push(aNumber);
      smsService.addSilentNumber(aNumber);
    }

    if (this._silentSmsObservers[aNumber].indexOf(aCallback) == -1) {
      this._silentSmsObservers[aNumber].push(aCallback);
    }
  },

  removeSilentSmsObserver: function removeSilentSmsObserver(aNumber, aCallback) {
    if (_debug) {
      LOG("removeSilentSmsObserver " + aNumber);
    }

    if (!this._silentSmsObservers || !this._silentSmsObservers[aNumber]) {
      if (_debug) {
        LOG("No observers for " + aNumber);
      }
      return;
    }

    let index = this._silentSmsObservers[aNumber].indexOf(aCallback);
    if (index != -1) {
      this._silentSmsObservers[aNumber].splice(index, 1);
      if (this._silentSmsObservers[aNumber].length == 0) {
        this._silentSmsObservers[aNumber] = null;
        this._silentNumbers.splice(this._silentNumbers.indexOf(aNumber), 1);
        smsService.removeSilentNumber(aNumber);
      }
    } else if (_debug) {
      LOG("No callback found for " + aNumber);
    }
  },

  _onSilentSms: function _onSilentSms(aSubject, aTopic, aData) {
    if (_debug) {
      LOG("Got silent message! " + aSubject.sender + " - " + aSubject.body);
    }

    let number = aSubject.sender;
    if (!number || this._silentNumbers.indexOf(number) == -1) {
      if (_debug) {
        LOG("No observers for " + number);
      }
      return;
    }

    // If the service ID is null it means that the payment provider asked the
    // user for her MSISDN, so we are in a MT only SMS auth flow. In this case
    // we manually set the service ID to the one corresponding with the SIM
    // that received the SMS.
    if (this._settings.paymentServiceId === null) {
      let i = 0;
      while(i < gRil.numRadioInterfaces) {
        if (this.iccInfo[i].iccId === aSubject.iccId) {
          this._settings.paymentServiceId = i;
          break;
        }
        i++;
      }
    }

    this._silentSmsObservers[number].forEach(function(callback) {
      callback(aSubject);
    });
  },

  _cleanUp: function _cleanUp() {
    if (_debug) {
      LOG("Cleaning up!");
    }

    if (!this._silentNumbers) {
      return;
    }

    while (this._silentNumbers.length) {
      let number = this._silentNumbers.pop();
      smsService.removeSilentNumber(number);
    }
    this._silentNumbers = null;
    this._silentSmsObservers = null;
    this._settings.cleanup();
    Services.obs.removeObserver(this._onSilentSms, kSilentSmsReceivedTopic);
  }
#endif
};

// We save the identifier of the DOM request, so we can dispatch the results
// of the payment flow to the appropriate content process.
addMessageListener("Payment:LoadShim", function receiveMessage(aMessage) {
  gRequestId = aMessage.json.requestId;
  PaymentProvider._init();
});

addEventListener("DOMWindowCreated", function(e) {
  content.wrappedJSObject.mozPaymentProvider = PaymentProvider;
});

#ifdef MOZ_B2G_RIL
// If the trusted dialog is not closed via paymentSuccess or paymentFailed
// a mozContentEvent with type 'cancel' is sent from the UI. We need to listen
// for this event to clean up the silent sms observers if any exists.
gBrowser.getContentWindow().addEventListener("mozContentEvent", function(e) {
  if (e.detail.type === "cancel") {
    PaymentProvider._cleanUp();
  }
});
#endif
