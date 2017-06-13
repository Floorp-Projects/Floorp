/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const DummyUIService = {
  canMakePayment: function(requestId) {
    let canMakeResponse = Cc["@mozilla.org/dom/payments/payment-canmake-action-response;1"].
                          createInstance(Ci.nsIPaymentCanMakeActionResponse);
    canMakeResponse.init(requestId, true);
    return canMakeResponse.QueryInterface(Ci.nsIPaymentActionResponse);
  },
  showPayment: function(requestId) {
    return null;
  },
  abortPayment: function(requestId) {
    let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"].
                        createInstance(Ci.nsIPaymentAbortActionResponse);
    abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
    return abortResponse.QueryInterface(Ci.nsIPaymentActionResponse);
  },
  completePayment: function(requestId) {
    return null;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
};

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
paymentSrv.setTestingUIService(DummyUIService.QueryInterface(Ci.nsIPaymentUIService));

addMessageListener('teardown', function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
