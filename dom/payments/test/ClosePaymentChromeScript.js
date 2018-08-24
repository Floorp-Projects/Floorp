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

addMessageListener("close-check", function() {
  const paymentEnum = paymentSrv.enumerate();
  if (paymentEnum.hasMoreElements()) {
    emitTestFail("Non-empty PaymentRequest queue in PaymentRequestService.");
  } else {
    emitTestPass("Got empty PaymentRequest queue in PaymentRequestService.");
  }
  sendAsyncMessage("close-check-complete");
});

var setPaymentNums = 0;

addMessageListener("payment-num-set", function() {
  setPaymentNums = 0;
  const paymentEnum = paymentSrv.enumerate();
  while (paymentEnum.hasMoreElements()) {
    setPaymentNums = setPaymentNums + 1;
    paymentEnum.getNext();
  }
  sendAsyncMessage("payment-num-set-complete");
});

addMessageListener("payment-num-check", function(expectedNumPayments) {
  const paymentEnum = paymentSrv.enumerate();
  let numPayments = 0;
  while (paymentEnum.hasMoreElements()) {
    numPayments = numPayments + 1;
    paymentEnum.getNext();
  }
  if (numPayments !== expectedNumPayments + setPaymentNums) {
    emitTestFail("Expected '" + expectedNumPayments +
                 "' PaymentRequests in PaymentRequestService" + ", but got '" +
                 numPayments + "'.");
  } else {
    emitTestPass("Got expected '" + numPayments +
                 "' PaymentRequests in PaymentRequestService.");
  }
  // force cleanup PaymentRequests for clear environment to next testcase.
  paymentSrv.cleanup();
  sendAsyncMessage("payment-num-check-complete");
});

var respondRequestId;

addMessageListener("reject-payment", (expectedError) => {
  try {
    const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                            createInstance(Ci.nsIGeneralResponseData);
    responseData.initData({});
    const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                            createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(respondRequestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
                      "",                 // payment method
                      responseData,       // payment method data
                      "",                 // payer name
                      "",                 // payer email
                      "");                // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
    emitTestPass("Reject PaymentRequest successfully");
  } catch (error) {
    if (expectedError) {
      if (error.name === "NS_ERROR_FAILURE") {
        emitTestPass("Got expected NS_ERROR_FAILURE when responding a closed PaymentRequest");
        sendAsyncMessage("reject-payment-complete");
        return;
      }
    }
    emitTestFail("Unexpected error '" + error.name +
                 "' when reponding a closed PaymentRequest");
  }
  sendAsyncMessage("reject-payment-complete");
});

addMessageListener("update-payment", () => {
  try {
    paymentSrv.changeShippingOption(respondRequestId, "");
    emitTestPass("Change shippingOption succefully");
  } catch (error) {
    emitTestFail("Unexpected error '" + error.name +
                 "' when changing the shipping option");
  }
  sendAsyncMessage("update-payment-complete");
});

const DummyUIService = {
  showPayment: (requestId => {respondRequestId = requestId}),
  abortPayment: (requestId) => {respondRequestId = requestId},
  completePayment: (requestId) => {respondRequestId = requestId},
  updatePayment: (requestId) => {respondRequestId = requestId},
  closePayment: (requestId) => {respondRequestId = requestId},
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

paymentSrv.setTestingUIService(DummyUIService.QueryInterface(Ci.nsIPaymentUIService));


addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
