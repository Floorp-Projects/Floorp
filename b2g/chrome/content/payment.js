/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS shim contains the callbacks to fire DOMRequest events for
// navigator.pay API within the payment processor's scope.

"use strict";

let _DEBUG = false;
function _debug(s) { dump("== Payment flow == " + s + "\n"); }
_debug("Frame script injected");

let { classes: Cc, interfaces: Ci, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

#ifdef MOZ_B2G_RIL
XPCOMUtils.defineLazyServiceGetter(this, "mobileConnection",
                                   "@mozilla.org/ril/content-helper;1",
                                   "nsIMobileConnectionProvider");

XPCOMUtils.defineLazyServiceGetter(this, "smsService",
                                   "@mozilla.org/sms/smsservice;1",
                                   "nsISmsService");

const kSilentSmsReceivedTopic = "silent-sms-received";

const MOBILEMESSAGECALLBACK_CID =
  Components.ID("{b484d8c9-6be4-4f94-ab60-c9c7ebcc853d}");

// In order to send messages through nsISmsService, we need to implement
// nsIMobileMessageCallback, as the WebSMS API implementation is not usable
// from JS.
function SilentSmsRequest() {
}
SilentSmsRequest.prototype = {
  __exposedProps__: {
    onsuccess: 'rw',
    onerror: 'rw'
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
    if (_DEBUG) {
      _debug("Silent message successfully sent");
    }
    this._onsuccess(aMessage);
  },

  notifySendMessageFailed: function notifySendMessageFailed(aError) {
    if (_DEBUG) {
      _debug("Error sending silent message " + aError);
    }
    this._onerror(aError);
  }
};
#endif

const kClosePaymentFlowEvent = "close-payment-flow-dialog";

let gRequestId;

let gBrowser = Services.wm.getMostRecentWindow("navigator:browser");

let PaymentProvider = {
#ifdef MOZ_B2G_RIL
  __exposedProps__: {
    paymentSuccess: 'r',
    paymentFailed: 'r',
    iccIds: 'r',
    sendSilentSms: 'r',
    observeSilentSms: 'r',
    removeSilentSmsObserver: 'r'
  },
#else
  __exposedProps__: {
    paymentSuccess: 'r',
    paymentFailed: 'r'
  },
#endif

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
    if (_DEBUG) {
      _debug("paymentSuccess " + aResult);
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
    if (_DEBUG) {
      _debug("paymentFailed " + aErrorMsg);
    }

    PaymentProvider._closePaymentFlowDialog(function notifyError() {
      if (!gRequestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aErrorMsg,
                                                requestId: gRequestId });
    });
  },

#ifdef MOZ_B2G_RIL
  get iccIds() {
    // Until bug 814629 is done, we only have support for a single SIM, so we
    // can only provide a single ICC ID. However, we return an array so the
    // payment provider facing API won't need to change once we support
    // multiple SIMs.
    return [mobileConnection.iccInfo.iccid];
  },

  _silentNumbers: null,
  _silentSmsObservers: null,

  sendSilentSms: function sendSilentSms(aNumber, aMessage) {
    if (_DEBUG) {
      _debug("Sending silent message " + aNumber + " - " + aMessage);
    }

    let request = new SilentSmsRequest();
    smsService.send(aNumber, aMessage, true, request);
    return request;
  },

  observeSilentSms: function observeSilentSms(aNumber, aCallback) {
    if (_DEBUG) {
      _debug("observeSilentSms " + aNumber);
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
    if (_DEBUG) {
      _debug("removeSilentSmsObserver " + aNumber);
    }

    if (!this._silentSmsObservers || !this._silentSmsObservers[aNumber]) {
      if (_DEBUG) {
        _debug("No observers for " + aNumber);
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
    } else if (_DEBUG) {
      _debug("No callback found for " + aNumber);
    }
  },

  _onSilentSms: function _onSilentSms(aSubject, aTopic, aData) {
    if (_DEBUG) {
      _debug("Got silent message! " + aSubject.sender + " - " + aSubject.body);
    }

    let number = aSubject.sender;
    if (!number || this._silentNumbers.indexOf(number) == -1) {
      if (_DEBUG) {
        _debug("No observers for " + number);
      }
      return;
    }

    this._silentSmsObservers[number].forEach(function(callback) {
      callback(aSubject);
    });
  },

  _cleanUp: function _cleanUp() {
    if (_DEBUG) {
      _debug("Cleaning up!");
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
    Services.obs.removeObserver(this._onSilentSms, kSilentSmsReceivedTopic);
  }
#endif
};

// We save the identifier of the DOM request, so we can dispatch the results
// of the payment flow to the appropriate content process.
addMessageListener("Payment:LoadShim", function receiveMessage(aMessage) {
  gRequestId = aMessage.json.requestId;
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
