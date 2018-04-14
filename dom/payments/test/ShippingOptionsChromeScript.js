/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}
function emitTestPass(message) {
  sendAsyncMessage("test-pass", message);
}

let expectedRequestOption = null;
let expectedUpdatedOption = null;
let changeShippingOption = null;

function showResponse(requestId) {
  const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                              createInstance(Ci.nsIGeneralResponseData);
  showResponseData.initData({});
  const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                          createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(requestId,
                    Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                    "testing-payment-method",   // payment method
                    showResponseData,           // payment method data
                    "Bill A. Pacheco",          // payer name
                    "",                         // payer email
                    "");                        // payer phone
  paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function showRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  const message = "request.shippingOption should be " + expectedRequestOption +
                  " when calling show(), but got " + request.shippingOption + ".";
  if (request.shippingOption != expectedRequestOption) {
    emitTestFail(message);
  } else {
    emitTestPass(message);
  }
  if (changeShippingOption) {
    paymentSrv.changeShippingOption(requestId, changeShippingOption);
  } else {
    showResponse(requestId);
  }
}

function updateRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  const message = "request.shippingOption should be " + expectedUpdatedOption +
                  " when calling updateWith(), but got " + request.shippingOption + ".";
  if (request.shippingOption != expectedUpdatedOption) {
    emitTestFail(message);
  } else {
    emitTestPass(message);
  }
  showResponse(requestId);
}

const TestingUIService = {
  showPayment: showRequest,
  abortPayment: function(requestId) {
  },
  completePayment: function(requestId) {
    let request = paymentSrv.getPaymentRequestById(requestId);
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                           createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  updatePayment: updateRequest,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
};

paymentSrv.setTestingUIService(TestingUIService.QueryInterface(Ci.nsIPaymentUIService));

addMessageListener("set-expected-results", function(results) {
  expectedRequestOption = results.requestResult;
  expectedUpdatedOption = results.responseResult;
  changeShippingOption = results.changeOptionResult;
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
