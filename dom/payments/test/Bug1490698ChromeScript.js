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

const billingAddress = Cc[
  "@mozilla.org/dom/payments/payment-address;1"
].createInstance(Ci.nsIPaymentAddress);
const addressLine = Cc["@mozilla.org/array;1"].createInstance(
  Ci.nsIMutableArray
);
const address = Cc["@mozilla.org/supports-string;1"].createInstance(
  Ci.nsISupportsString
);
address.data = "Easton Ave";
addressLine.appendElement(address);
billingAddress.init(
  "USA", // country
  addressLine, // address line
  "CA", // region
  "CA", // region code
  "San Bruno", // city
  "", // dependent locality
  "94066", // postal code
  "123456", // sorting code
  "", // organization
  "Bill A. Pacheco", // recipient
  "+14344413879"
); // phone

function acceptPayment(requestId) {
  const basiccardResponseData = Cc[
    "@mozilla.org/dom/payments/basiccard-response-data;1"
  ].createInstance(Ci.nsIBasicCardResponseData);
  const showResponse = Cc[
    "@mozilla.org/dom/payments/payment-show-action-response;1"
  ].createInstance(Ci.nsIPaymentShowActionResponse);
  basiccardResponseData.initData(
    "Bill A. Pacheco", // cardholderName
    "4916855166538720", // cardNumber
    "01", // expiryMonth
    "2024", // expiryYear
    "180", // cardSecurityCode
    billingAddress
  ); // billingAddress
  showResponse.init(
    requestId,
    Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
    "basic-card", // payment method
    basiccardResponseData, // payment method data
    "Bill A. Pacheco", // payer name
    "", // payer email
    ""
  ); // payer phone
  paymentSrv.respondPayment(
    showResponse.QueryInterface(Ci.nsIPaymentActionResponse)
  );
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
    acceptPayment(requestId);
  },
  abortPaymen(requestId) {
    this.requestId = requestId;
  },
  completePayment(requestId) {
    this.requestId = requestId;
    let completeResponse = Cc[
      "@mozilla.org/dom/payments/payment-complete-action-response;1"
    ].createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(
      requestId,
      Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED
    );
    paymentSrv.respondPayment(
      completeResponse.QueryInterface(Ci.nsIPaymentActionResponse)
    );
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

addMessageListener("start-test", function(testName) {
  DummyUIService.testName = testName;
  sendAsyncMessage("start-test-complete");
});

addMessageListener("finish-test", function() {
  DummyUIService.testName = "";
  sendAsyncMessage("finish-test-complete");
});

addMessageListener("interact-with-payment", function() {
  if (DummyUIService.requestId === "") {
    emitTestFail(`${DummyUIService.testName}: Unexpected empty requestId`);
  }
  try {
    acceptPayment(DummyUIService.requestId);
    emitTestFail(
      `${DummyUIService.testName}: Got unexpected success when accepting PaymentRequest.`
    );
  } catch (err) {
    if (err.name !== "NS_ERROR_FAILURE") {
      emitTestFail(
        `${DummyUIService.testName}: Got unexpected '${err.name}' when accepting PaymentRequest.`
      );
    } else {
      emitTestPass(
        `${DummyUIService.testName}: Got expected 'NS_ERROR_FAILURE' when accepting PaymentRequest.`
      );
    }
  }

  try {
    rejectPayment(DummyUIService.requestId);
    emitTestFail(
      `${DummyUIService.testName}: Got unexpected success when rejecting PaymentRequest.`
    );
  } catch (err) {
    if (err.name !== "NS_ERROR_FAILURE") {
      emitTestFail(
        `${DummyUIService.testName}: Got unexpected '${err.name}' when rejecting PaymentRequest.`
      );
    } else {
      emitTestPass(
        `${DummyUIService.testName}: Got expected 'NS_ERROR_FAILURE' when rejecting PaymentRequest.`
      );
    }
  }

  try {
    paymentSrv.changeShippingOption(
      DummyUIService.requestId,
      "error shippping option"
    );
    emitTestFail(
      `${DummyUIService.testName}: Got unexpected success when changing shippingOption.`
    );
  } catch (err) {
    if (err.name !== "NS_ERROR_FAILURE") {
      emitTestFail(
        `${DummyUIService.testName}: Got unexpected '${err.name}' when changin shippingOption.`
      );
    } else {
      emitTestPass(
        `${DummyUIService.testName}: Got expected 'NS_ERROR_FAILURE' when changing shippingOption.`
      );
    }
  }

  try {
    paymentSrv.changeShippingOption(DummyUIService.requestId, billingAddress);
    emitTestFail(
      `${DummyUIService.testName}: Got unexpected success when changing shippingAddress.`
    );
  } catch (err) {
    if (err.name !== "NS_ERROR_FAILURE") {
      emitTestFail(
        `${DummyUIService.testName}: Got unexpected '${err.name}' when changing shippingAddress.`
      );
    } else {
      emitTestPass(
        `${DummyUIService.testName}: Got expected 'NS_ERROR_FAILURE' when changing shippingAddress.`
      );
    }
  }
  sendAsyncMessage("interact-with-payment-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
