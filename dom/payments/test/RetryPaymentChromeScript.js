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

function acceptPayment(requestId, mode) {
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
  if (mode === "show") {
    showResponse.init(
      requestId,
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "basic-card", // payment method
      basiccardResponseData, // payment method data
      "Bill A. Pacheco", // payer name
      "", // payer email
      ""
    ); // payer phone
  }
  if (mode == "retry") {
    showResponse.init(
      requestId,
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "basic-card", // payment method
      basiccardResponseData, // payment method data
      "Bill A. Pacheco", // payer name
      "bpacheco@test.org", // payer email
      "+123456789"
    ); // payer phone
  }
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

function checkAddressErrors(testName, errors) {
  if (!errors) {
    emitTestFail(
      `${testName}: Expect non-null shippingAddressErrors, but got null.`
    );
    return;
  }
  for (const [key, msg] of Object.entries(errors)) {
    const expected = `${key} error`;
    if (msg !== expected) {
      emitTestFail(
        `${testName}: Expected '${expected}' on shippingAddressErrors.${key}, but got '${msg}'.`
      );
      return;
    }
  }
}

function checkPayerErrors(testName, errors) {
  if (!errors) {
    emitTestFail(`${testName}: Expect non-null payerErrors, but got null.`);
    return;
  }
  for (const [key, msg] of Object.entries(errors)) {
    const expected = `${key} error`;
    if (msg !== expected) {
      emitTestFail(
        `${testName}: Expected '${expected}' on payerErrors.${key}, but got '${msg}'.`
      );
      return;
    }
  }
}

function checkPaymentMethodErrors(testName, errors) {
  if (!errors) {
    emitTestFail(
      `${testName} :Expect non-null paymentMethodErrors, but got null.`
    );
    return;
  }
  for (const [key, msg] of Object.entries(errors)) {
    const expected = `method ${key} error`;
    if (msg !== expected) {
      emitTestFail(
        `${testName}: Expected '${expected}' on paymentMethodErrors.${key}, but got '${msg}'.`
      );
      return;
    }
  }
}

const DummyUIService = {
  testName: "",
  rejectRetry: false,
  showPayment(requestId) {
    acceptPayment(requestId, "show");
  },
  abortPaymen(requestId) {
    respondRequestId = requestId;
  },
  completePayment(requestId) {
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
    const payment = paymentSrv.getPaymentRequestById(requestId);
    if (payment.paymentDetails.error !== "error") {
      emitTestFail(
        "Expect 'error' on details.error, but got '" +
          payment.paymentDetails.error +
          "'"
      );
    }
    checkAddressErrors(
      this.testName,
      payment.paymentDetails.shippingAddressErrors
    );
    checkPayerErrors(this.testName, payment.paymentDetails.payerErrors);
    checkPaymentMethodErrors(
      this.testName,
      payment.paymentDetails.paymentMethodErrors
    );
    if (this.rejectRetry) {
      rejectPayment(requestId);
    } else {
      acceptPayment(requestId, "retry");
    }
  },
  closePayment: requestId => {
    respondRequestId = requestId;
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

addMessageListener("reject-retry", function() {
  DummyUIService.rejectRetry = true;
  sendAsyncMessage("reject-retry-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
