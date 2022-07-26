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
  sendAsyncMessage("test-fail", `${DummyUIService.testName}: ${message}`);
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

const specialAddress = Cc[
  "@mozilla.org/dom/payments/payment-address;1"
].createInstance(Ci.nsIPaymentAddress);
const specialAddressLine = Cc["@mozilla.org/array;1"].createInstance(
  Ci.nsIMutableArray
);
const specialData = Cc["@mozilla.org/supports-string;1"].createInstance(
  Ci.nsISupportsString
);
specialData.data = ":$%@&*";
specialAddressLine.appendElement(specialData);
specialAddress.init(
  "USA", // country
  specialAddressLine, // address line
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

const basiccardResponseData = Cc[
  "@mozilla.org/dom/payments/basiccard-response-data;1"
].createInstance(Ci.nsIBasicCardResponseData);

const basiccardChangeDetails = Cc[
  "@mozilla.org/dom/payments/basiccard-change-details;1"
].createInstance(Ci.nsIBasicCardChangeDetails);

const showResponse = Cc[
  "@mozilla.org/dom/payments/payment-show-action-response;1"
].createInstance(Ci.nsIPaymentShowActionResponse);

function abortPaymentResponse(requestId) {
  let abortResponse = Cc[
    "@mozilla.org/dom/payments/payment-abort-action-response;1"
  ].createInstance(Ci.nsIPaymentAbortActionResponse);
  abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
  paymentSrv.respondPayment(
    abortResponse.QueryInterface(Ci.nsIPaymentActionResponse)
  );
}

function completePaymentResponse(requestId) {
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
}

function showRequest(requestId) {
  if (DummyUIService.showAction === "payment-method-change") {
    basiccardChangeDetails.initData(billingAddress);
    try {
      paymentSrv.changePaymentMethod(
        requestId,
        "basic-card",
        basiccardChangeDetails.QueryInterface(Ci.nsIMethodChangeDetails)
      );
    } catch (error) {
      emitTestFail(
        `Unexpected error (${error.name}) when calling PaymentRequestService::changePaymentMethod`
      );
    }
    return;
  }
  if (DummyUIService.showAction === "detailBasicCardResponse") {
    try {
      basiccardResponseData.initData(
        "Bill A. Pacheco", // cardholderName
        "4916855166538720", // cardNumber
        "01", // expiryMonth
        "2024", // expiryYear
        "180", // cardSecurityCode
        billingAddress
      ); // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
  }
  if (DummyUIService.showAction === "simpleBasicCardResponse") {
    try {
      basiccardResponseData.initData(
        "", // cardholderName
        "4916855166538720", // cardNumber
        "", // expiryMonth
        "", // expiryYear
        "", // cardSecurityCode
        null
      ); // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
  }
  if (DummyUIService.showAction === "specialAddressResponse") {
    try {
      basiccardResponseData.initData(
        "Bill A. Pacheco", // cardholderName
        "4916855166538720", // cardNumber
        "01", // expiryMonth
        "2024", // expiryYear
        "180", // cardSecurityCode
        specialAddress
      ); // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
  }
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

const DummyUIService = {
  testName: "",
  showAction: "",
  showPayment: showRequest,
  abortPayment: abortPaymentResponse,
  completePayment: completePaymentResponse,
  updatePayment: requestId => {
    try {
      basiccardResponseData.initData(
        "Bill A. Pacheco", // cardholderName
        "4916855166538720", // cardNumber
        "01", // expiryMonth
        "2024", // expiryYear
        "180", // cardSecurityCode
        billingAddress
      ); // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
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
  },
  closePayment: requestId => {},
  QueryInterface: ChromeUtils.generateQI(["nsIPaymentUIService"]),
};

paymentSrv.setTestingUIService(
  DummyUIService.QueryInterface(Ci.nsIPaymentUIService)
);

addMessageListener("set-detailed-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.showAction = "detailBasicCardResponse";
  sendAsyncMessage("set-detailed-ui-service-complete");
});

addMessageListener("set-simple-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.showAction = "simpleBasicCardResponse";
  sendAsyncMessage("set-simple-ui-service-complete");
});

addMessageListener("set-special-address-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.showAction = "specialAddressResponse";
  sendAsyncMessage("set-special-address-ui-service-complete");
});

addMessageListener("method-change-to-basic-card", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.showAction = "payment-method-change";
  sendAsyncMessage("method-change-to-basic-card-complete");
});

addMessageListener("error-response-test", function(testName) {
  // test empty cardNumber
  try {
    basiccardResponseData.initData("", "", "", "", "", null);
    emitTestFail(
      "BasicCardResponse should not be initialized with empty cardNumber."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "Empty cardNumber expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }

  // test invalid expiryMonth 123
  try {
    basiccardResponseData.initData("", "4916855166538720", "123", "", "", null);
    emitTestFail(
      "BasicCardResponse should not be initialized with invalid expiryMonth '123'."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "expiryMonth 123 expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }
  // test invalid expiryMonth 99
  try {
    basiccardResponseData.initData("", "4916855166538720", "99", "", "", null);
    emitTestFail(
      "BasicCardResponse should not be initialized with invalid expiryMonth '99'."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "expiryMonth 99 xpected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }
  // test invalid expiryMonth ab
  try {
    basiccardResponseData.initData("", "4916855166538720", "ab", "", "", null);
    emitTestFail(
      "BasicCardResponse should not be initialized with invalid expiryMonth 'ab'."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "expiryMonth ab expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }
  // test invalid expiryYear abcd
  try {
    basiccardResponseData.initData(
      "",
      "4916855166538720",
      "",
      "abcd",
      "",
      null
    );
    emitTestFail(
      "BasicCardResponse should not be initialized with invalid expiryYear 'abcd'."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "expiryYear abcd expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }
  // test invalid expiryYear 11111
  try {
    basiccardResponseData.initData(
      "",
      "4916855166538720",
      "",
      "11111",
      "",
      null
    );
    emitTestFail(
      "BasicCardResponse should not be initialized with invalid expiryYear '11111'."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "expiryYear 11111 expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }

  const responseData = Cc[
    "@mozilla.org/dom/payments/general-response-data;1"
  ].createInstance(Ci.nsIGeneralResponseData);
  try {
    responseData.initData({});
  } catch (e) {
    emitTestFail("Fail to initialize response data with empty object.");
  }

  try {
    showResponse.init(
      "testid",
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "basic-card", // payment method
      responseData, // payment method data
      "Bill A. Pacheco", // payer name
      "", // payer email
      ""
    ); // payer phone
    emitTestFail(
      "nsIPaymentShowActionResponse should not be initialized with basic-card method and nsIGeneralResponseData."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "ShowResponse init expected 'NS_ERROR_FAILURE', but got " + e.name + "."
      );
    }
  }
  sendAsyncMessage("error-response-test-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
