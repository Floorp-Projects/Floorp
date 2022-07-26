/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-script */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const paymentSrv = Cc[
  "@mozilla.org/dom/payments/payment-request-service;1"
].getService(Ci.nsIPaymentRequestService);

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}
function emitTestPass(message) {
  sendAsyncMessage("test-pass", message);
}

function rejectPayment(requestId) {
  const responseData = Cc[
    "@mozilla.org/dom/payments/general-response-data;1"
  ].createInstance(Ci.nsIGeneralResponseData);
  responseData.initData({});
  const showResponse = Cc[
    "@mozilla.org/dom/payments/payment-show-action-response;1"
  ].createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(
    requestId,
    Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
    "", // payment method
    responseData, // payment method data
    "", // payer name
    "", // payer email
    ""
  ); // payer phone
  paymentSrv.respondPayment(
    showResponse.QueryInterface(Ci.nsIPaymentActionResponse)
  );
}

const DummyUIService = {
  testName: "",
  requestId: "",
  showPayment(requestId) {
    this.requestId = requestId;
    sendAsyncMessage("showing-payment", { data: "successful" });
  },
  abortPayment(requestId) {
    this.requestId = requestId;
  },
  completePayment(requestId) {
    this.requestId = requestId;
  },
  updatePayment(requestId) {
    this.requestId = requestId;
  },
  closePayment(requestId) {
    this.requestId = requestId;
  },
  QueryInterface: ChromeUtils.generateQI(["nsIPaymentUIService"]),
};

paymentSrv.setTestingUIService(
  DummyUIService.QueryInterface(Ci.nsIPaymentUIService)
);

addMessageListener("reject-payment", function() {
  rejectPayment(DummyUIService.requestId);
  sendAsyncMessage("reject-payment-complete");
});

addMessageListener("start-test", function(testName) {
  DummyUIService.testName = testName;
  sendAsyncMessage("start-test-complete");
});

addMessageListener("finish-test", function() {
  DummyUIService.testName = "";
  sendAsyncMessage("finish-test-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
