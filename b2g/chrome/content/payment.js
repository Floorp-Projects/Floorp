/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This JS shim contains the callbacks to fire DOMRequest events for
// navigator.pay API within the payment processor's scope.

"use strict";

dump("======================= payment.js ======================= \n");

let { classes: Cc, interfaces: Ci, utils: Cu }  = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
#endif


const kClosePaymentFlowEvent = "close-payment-flow-dialog";

let _requestId;

let PaymentProvider = {

  __exposedProps__: {
    paymentSuccess: 'r',
    paymentFailed: 'r',
    iccIds: 'r'
  },

  _closePaymentFlowDialog: function _closePaymentFlowDialog(aCallback) {
    // After receiving the payment provider confirmation about the
    // successful or failed payment flow, we notify the UI to close the
    // payment flow dialog and return to the caller application.
    let id = kClosePaymentFlowEvent + "-" + uuidgen.generateUUID().toString();

    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    let content = browser.getContentWindow();
    if (!content) {
      return;
    }

    let detail = {
      type: kClosePaymentFlowEvent,
      id: id,
      requestId: _requestId
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

    browser.shell.sendChromeEvent(detail);
  },

  paymentSuccess: function paymentSuccess(aResult) {
    this._closePaymentFlowDialog(function notifySuccess() {
      if (!_requestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Success", { result: aResult,
                                                 requestId: _requestId });
    });
  },

  paymentFailed: function paymentFailed(aErrorMsg) {
    this._closePaymentFlowDialog(function notifyError() {
      if (!_requestId) {
        return;
      }
      cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aErrorMsg,
                                                requestId: _requestId });
    });
  },

  get iccIds() {
#ifdef MOZ_B2G_RIL
    // Until bug 814629 is done, we only have support for a single SIM, so we
    // can only provide a single ICC ID. However, we return an array so the
    // payment provider facing API won't need to change once we support
    // multiple SIMs.
    return [mobileConnection.iccInfo.iccid];
#else
    return null;
#endif
  },

};

// We save the identifier of the DOM request, so we can dispatch the results
// of the payment flow to the appropriate content process.
addMessageListener("Payment:LoadShim", function receiveMessage(aMessage) {
  _requestId = aMessage.json.requestId;
});

addEventListener("DOMWindowCreated", function(e) {
  content.wrappedJSObject.mozPaymentProvider = PaymentProvider;
});
