/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");


const PREF_DEBUG = "dom.payment.debug";

let _debug;
try {
  _debug = Services.prefs.getPrefType(PREF_DEBUG) == Ci.nsIPrefBranch.PREF_BOOL
           && Services.prefs.getBoolPref(PREF_DEBUG);
} catch(e) {
  _debug = false;
}

function DEBUG(s) {
  if (!_debug) {
    return;
  }
  dump("== Payment Provider == " + s + "\n");
}

function DEBUG_E(s) {
  dump("== Payment Provider ERROR == " + s + "\n");
}

const kPaymentFlowCancelled = "payment-flow-cancelled";

function PaymentProvider() {
}

PaymentProvider.prototype = {
  init: function(aWindow) {
    let docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    this._requestId = docshell.paymentRequestId;
    this._oncancelObserver = this.oncancel.bind(this);
    Services.obs.addObserver(this._oncancelObserver,
                             kPaymentFlowCancelled,
                             false);
    this._strategy = Cc["@mozilla.org/payment/provider-strategy;1"]
                       .createInstance(Ci.nsIPaymentProviderStrategy);
    this._window = aWindow;
  },

  paymentSuccess: function(aResult) {
    _debug && DEBUG("paymentSuccess " + aResult);
    let glue = Cc["@mozilla.org/payment/ui-glue;1"]
                 .createInstance(Ci.nsIPaymentUIGlue);
    glue.closePaymentFlow(this._requestId).then(() => {
      if (!this._requestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Success", { result: aResult,
                                                 requestId: this._requestId });
    });
  },

  paymentFailed: function(aError) {
    _debug && DEBUG("paymentFailed " + aError);
    let glue = Cc["@mozilla.org/payment/ui-glue;1"]
                 .createInstance(Ci.nsIPaymentUIGlue);
    glue.closePaymentFlow(this._requestId).then(() => {
      if (!this._requestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aError,
                                                requestId: this._requestId });
    });

  },

  get paymentServiceId() {
    return this._strategy.paymentServiceId;
  },

  set paymentServiceId(aServiceId) {
    this._strategy.paymentServiceId = aServiceId;
  },

  /**
   * We expose to the payment provider the information of all the SIMs
   * available in the device. iccInfo is an object of this form:
   * {
   *   "serviceId1": {
   *      mcc: <string>,
   *      mnc: <string>,
   *      iccId: <string>,
   *      dataPrimary: <boolean>
   *    },
   *   "serviceIdN": {...}
   * }
   */
  get iccInfo() {
    return this._strategy.iccInfo;
  },

  oncancel: function() {
    _debug && DEBUG("Cleaning up!");

    this._strategy.cleanup();
    Services.obs.removeObserver(this._oncancelObserver, kPaymentFlowCancelled);
    if (this._cleanup) {
      this._cleanup();
    }
  },

  classID: Components.ID("{82144756-72ab-45b7-8621-f3dad431dd2f}"),

  contractID: "@mozilla.org/payment/provider;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIObserver,
                                         Ci.nsIDOMGlobalPropertyInitializer])
};

#if defined(MOZ_B2G_RIL) || defined(MOZ_WIDGET_ANDROID)

XPCOMUtils.defineLazyServiceGetter(this, "smsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

const kSilentSmsReceivedTopic = "silent-sms-received";

// In order to send messages through nsISmsService, we need to implement
// nsIMobileMessageCallback, as the WebSMS API implementation is not usable
// from JS.
function SilentSmsRequest(aDOMRequest) {
  this.request = aDOMRequest;
}

SilentSmsRequest.prototype = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMobileMessageCallback]),

  classID: Components.ID("{1d58889c-5660-4cca-a8fd-97ef63e5d3b2}"),

  notifyMessageSent: function notifyMessageSent(aMessage) {
    _debug && DEBUG("Silent message successfully sent");
    Services.DOMRequest.fireSuccessAsync(this.request, aMessage);
  },

  notifySendMessageFailed: function notifySendMessageFailed(aError) {
    DEBUG_E("Error sending silent message " + aError);
    Services.DOMRequest.fireErrorAsync(this.request, aError);
  }
};

PaymentProvider.prototype._silentNumbers = null;

PaymentProvider.prototype._silentSmsObservers = null;

PaymentProvider.prototype.sendSilentSms = function(aNumber, aMessage) {
  _debug && DEBUG("Sending silent message " + aNumber + " - " + aMessage);

  let request = Services.DOMRequest.createRequest(this._window);

  if (this._strategy.paymentServiceId === null) {
    DEBUG_E("No payment service ID set. Cannot send silent SMS");
    Services.DOMRequest.fireErrorAsync(request,
                                       "NO_PAYMENT_SERVICE_ID");
    return request;
  }

  let smsRequest = new SilentSmsRequest(request);
  smsService.send(this._strategy.paymentServiceId, aNumber, aMessage, true,
                  smsRequest);
  return request;
};

PaymentProvider.prototype.observeSilentSms = function(aNumber, aCallback) {
  _debug && DEBUG("observeSilentSms " + aNumber);

  if (!this._silentSmsObservers) {
    this._silentSmsObservers = {};
    this._silentNumbers = [];
    this._onSilentSmsObserver = this._onSilentSms.bind(this);
    Services.obs.addObserver(this._onSilentSmsObserver,
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
  return;
};

PaymentProvider.prototype.removeSilentSmsObserver = function(aNumber, aCallback) {
  _debug && DEBUG("removeSilentSmsObserver " + aNumber);

  if (!this._silentSmsObservers || !this._silentSmsObservers[aNumber]) {
    _debug && DEBUG("No observers for " + aNumber);
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
    DEBUG("No callback found for " + aNumber);
  }
  return;
};

PaymentProvider.prototype._onSilentSms = function(aSubject, aTopic, aData) {
  _debug && DEBUG("Got silent message! " + aSubject.sender + " - " + aSubject.body);

  let number = aSubject.sender;
  if (!number || this._silentNumbers.indexOf(number) == -1) {
    _debug && DEBUG("No observers for " + number);
    return;
  }

  // If the service ID is null it means that the payment provider asked the
  // user for her MSISDN, so we are in a MT only SMS auth flow. In this case
  // we manually set the service ID to the one corresponding with the SIM
  // that received the SMS.
  if (this._strategy.paymentServiceId === null) {
    let i = 0;
    while(i < gRil.numRadioInterfaces) {
      if (this.iccInfo[i].iccId === aSubject.iccId) {
        this._strategy.paymentServiceId = i;
        break;
      }
      i++;
    }
  }

  this._silentSmsObservers[number].forEach(function(callback) {
    callback(aSubject);
  });
};

PaymentProvider.prototype._cleanup = function() {
  if (!this._silentNumbers) {
    return;
  }

  while (this._silentNumbers.length) {
    let number = this._silentNumbers.pop();
    smsService.removeSilentNumber(number);
  }
  this._silentNumbers = null;
  this._silentSmsObservers = null;
  Services.obs.removeObserver(this._onSilentSmsObserver,
                              kSilentSmsReceivedTopic);
};

#else

PaymentProvider.prototype.sendSilentSms = function(aNumber, aMessage) {
  throw Cr.NS_ERROR_NOT_IMPLEMENTED;
};

PaymentProvider.prototype.observeSilentSms = function(aNumber, aCallback) {
  throw Cr.NS_ERROR_NOT_IMPLEMENTED;
};

PaymentProvider.prototype.removeSilentSmsObserver = function(aNumber, aCallback) {
  throw Cr.NS_ERROR_NOT_IMPLEMENTED;
};

#endif

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PaymentProvider]);
