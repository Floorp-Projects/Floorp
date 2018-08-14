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

addMessageListener("cleanup-check", function() {
  const paymentEnum = paymentSrv.enumerate();
  if (paymentEnum.hasMoreElements()) {
    emitTestFail("Non-empty PaymentRequest queue in PaymentRequestService.");
  } else {
    emitTestPass("Got empty PaymentRequest queue in PaymentRequestService.");
  }
  sendAsyncMessage("cleanup-check-complete");
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

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
