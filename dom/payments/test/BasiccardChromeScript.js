/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}

const billingAddress = Cc["@mozilla.org/dom/payments/payment-address;1"].
                           createInstance(Ci.nsIPaymentAddress);
const addressLine = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
const address = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
address.data = "Easton Ave";
addressLine.appendElement(address);
billingAddress.init("USA",              // country
                     addressLine,        // address line
                     "CA",               // region
                     "San Bruno",        // city
                     "",                 // dependent locality
                     "94066",            // postal code
                     "123456",           // sorting code
                     "en",               // language code
                     "",                 // organization
                     "Bill A. Pacheco",  // recipient
                     "+14344413879"); // phone

const basiccardResponseData = Cc["@mozilla.org/dom/payments/basiccard-response-data;1"].
                                 createInstance(Ci.nsIBasicCardResponseData);

const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                        createInstance(Ci.nsIPaymentShowActionResponse);

function abortPaymentResponse(requestId) {
  let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"].
                         createInstance(Ci.nsIPaymentAbortActionResponse);
  abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
  paymentSrv.respondPayment(abortResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function completePaymentResponse(requestId) {
  let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                            createInstance(Ci.nsIPaymentCompleteActionResponse);
  completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
  paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

const detailedResponseUI = {
  showPayment: function(requestId) {
    try {
      basiccardResponseData.initData("Bill A. Pacheco",  // cardholderName
                                     "4916855166538720", // cardNumber
                                     "01",               // expiryMonth
                                     "2024",             // expiryYear
                                     "180",              // cardSecurityCode
                                     billingAddress);   // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "basic-card",         // payment method
                      basiccardResponseData,// payment method data
                      "Bill A. Pacheco",    // payer name
                      "",                   // payer email
                      "");                  // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  abortPayment: abortPaymentResponse,
  completePayment: completePaymentResponse,
  updatePayment: function(requestId) {
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

const simpleResponseUI = {
  showPayment: function(requestId) {
    try {
      basiccardResponseData.initData("",                 // cardholderName
                                     "4916855166538720", // cardNumber
                                     "",                 // expiryMonth
                                     "",                 // expiryYear
                                     "",                 // cardSecurityCode
                                     null);              // billingAddress
    } catch (e) {
      emitTestFail("Fail to initialize basic card response data.");
    }
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "basic-card",         // payment method
                      basiccardResponseData,// payment method data
                      "Bill A. Pacheco",    // payer name
                      "",                   // payer email
                      "");                  // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  abortPayment: abortPaymentResponse,
  completePayment: completePaymentResponse,
  updatePayment: function(requestId) {
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

addMessageListener("set-detailed-ui-service", function() {
  paymentSrv.setTestingUIService(detailedResponseUI.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("set-simple-ui-service", function() {
  paymentSrv.setTestingUIService(simpleResponseUI.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("error-response-test", function() {
  // test empty cardNumber
  try {
    basiccardResponseData.initData("", "", "", "", "", null);
    emitTestFail("BasicCardResponse should not be initialized with empty cardNumber.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("Empty cardNumber expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }

  // test invalid expiryMonth 123
  try {
    basiccardResponseData.initData("", "4916855166538720", "123", "", "", null);
    emitTestFail("BasicCardResponse should not be initialized with invalid expiryMonth '123'.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("expiryMonth 123 expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }
  // test invalid expiryMonth 99
  try {
    basiccardResponseData.initData("", "4916855166538720", "99", "", "", null);
    emitTestFail("BasicCardResponse should not be initialized with invalid expiryMonth '99'.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("expiryMonth 99 xpected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }
  // test invalid expiryMonth ab
  try {
    basiccardResponseData.initData("", "4916855166538720", "ab", "", "", null);
    emitTestFail("BasicCardResponse should not be initialized with invalid expiryMonth 'ab'.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("expiryMonth ab expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }
  // test invalid expiryYear abcd
  try {
    basiccardResponseData.initData("", "4916855166538720", "", "abcd", "", null);
    emitTestFail("BasicCardResponse should not be initialized with invalid expiryYear 'abcd'.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("expiryYear abcd expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }
  // test invalid expiryYear 11111
  try {
    basiccardResponseData.initData("", "4916855166538720", "", "11111", "", null);
    emitTestFail("BasicCardResponse should not be initialized with invalid expiryYear '11111'.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("expiryYear 11111 expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }


  const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                          createInstance(Ci.nsIGeneralResponseData);
  try {
    responseData.initData({});
  } catch (e) {
    emitTestFail("Fail to initialize response data with empty object.");
  }

  try {
    showResponse.init("testid",
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "basic-card",         // payment method
                      responseData,         // payment method data
                      "Bill A. Pacheco",    // payer name
                      "",                   // payer email
                      "");                  // payer phone
    emitTestFail("nsIPaymentShowActionResponse should not be initialized with basic-card method and nsIGeneralResponseData.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("ShowResponse init expected 'NS_ERROR_FAILURE', but got " + e.name + ".");
    }
  }
  sendAsyncMessage("error-response-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
