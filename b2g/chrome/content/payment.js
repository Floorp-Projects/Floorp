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

const kClosePaymentFlowEvent = "close-payment-flow-dialog";

let requestId;

function paymentSuccess(aResult) {
  closePaymentFlowDialog(function notifySuccess() {
    if (!requestId) {
      return;
    }
    cpmm.sendAsyncMessage("Payment:Success", { result: aResult,
                                               requestId: requestId });
  });
}

function paymentFailed(aErrorMsg) {
  closePaymentFlowDialog(function notifyError() {
    if (!requestId) {
      return;
    }
    cpmm.sendAsyncMessage("Payment:Failed", { errorMsg: aErrorMsg,
                                              requestId: requestId });
  });
}

function closePaymentFlowDialog(aCallback) {
  // After receiving the payment provider confirmation about the
  // successful or failed payment flow, we notify the UI to close the
  // payment flow dialog and return to the caller application.
  let randomId = uuidgen.generateUUID().toString();
  let id = kClosePaymentFlowEvent + "-" + randomId;

  let browser = Services.wm.getMostRecentWindow("navigator:browser");
  let content = browser.getContentWindow();
  if (!content) {
    return;
  }

  let detail = {
    type: kClosePaymentFlowEvent,
    id: id,
    requestId: requestId
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
}

// We save the identifier of the DOM request, so we can dispatch the results
// of the payment flow to the appropriate content process.
addMessageListener("Payment:LoadShim", function receiveMessage(aMessage) {
  requestId = aMessage.json.requestId;
});

addEventListener("DOMWindowCreated", function(e) {
  content.wrappedJSObject.paymentSuccess = paymentSuccess;
  content.wrappedJSObject.paymentFailed = paymentFailed;
});
