/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

const UIService = {
  showPayment: function(requestId) {
    paymentSrv.changeShippingOption(requestId, "");
  },
  abortPayment: function(requestId) {
    let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"].
                           createInstance(Ci.nsIPaymentAbortActionResponse);
    abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
    paymentSrv.respondPayment(abortResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  completePayment: function(requestId) {
    const completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                           createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  updatePayment: function(requestId) {
    const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                                createInstance(Ci.nsIGeneralResponseData);
    showResponseData.initData({ paymentToken: "6880281f-0df3-4b8e-916f-66575e2457c1",});

    const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                      createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "https://example.com",   // payment method
                      showResponseData,           // payment method data
                      "Bill A. Pacheco",          // payer name
                      "",                         // payer email
                      "");                        // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}

addMessageListener("set-ui-service", function() {
  paymentSrv.setTestingUIService(UIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  sendAsyncMessage("teardown-complete");
});
