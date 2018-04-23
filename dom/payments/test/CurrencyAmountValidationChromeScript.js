/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

const InvalidDetailsUIService = {
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
  },
  updatePayment: function(requestId) {
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),

};

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}

function checkLowerCaseCurrency() {
  const paymentEnum = paymentSrv.enumerate();
  if (!paymentEnum.hasMoreElements()) {
    emitTestFail("PaymentRequestService should have at least one payment request.");
  }
  while (paymentEnum.hasMoreElements()) {
    let payRequest = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
    if (!payRequest) {
      emitTestFail("Fail to get existing payment request.");
      break;
    }
    if (payRequest.paymentDetails.totalItem.amount.currency != "USD") {
      emitTestFail("Currency of PaymentItem total should be 'USD', but got " +
                   payRequest.paymentDetails.totalItem.amount.currency + ".");
    }
  }
  paymentSrv.cleanup();
  sendAsyncMessage("check-complete");
}

addMessageListener("check-lower-case-currency", checkLowerCaseCurrency);

addMessageListener("set-update-with-invalid-details-ui-service", function() {
  paymentSrv.setTestingUIService(InvalidDetailsUIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  sendAsyncMessage("teardown-complete");
});
