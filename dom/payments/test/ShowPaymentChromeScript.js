/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
let expectedCompleteStatus = null;
let expectedShowAction = "accept";
let expectedUpdateAction = "accept";

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}
function emitTestPass(message) {
  sendAsyncMessage("test-pass", message);
}

const shippingAddress = Cc["@mozilla.org/dom/payments/payment-address;1"].
                           createInstance(Ci.nsIPaymentAddress);
const addressLine = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
const address = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
address.data = "Easton Ave";
addressLine.appendElement(address);
shippingAddress.init("USA",              // country
                     addressLine,        // address line
                     "CA",               // region
                     "San Bruno",        // city
                     "Test locality",    // dependent locality
                     "94066",            // postal code
                     "123456",           // sorting code
                     "en",               // language code
                     "Testing Org",      // organization
                     "Bill A. Pacheco",  // recipient
                     "+1-434-441-3879"); // phone

function acceptShow(requestId) {
  const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                          createInstance(Ci.nsIGeneralResponseData);
  responseData.initData({ paymentToken: "6880281f-0df3-4b8e-916f-66575e2457c1",});
  let showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                        createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(requestId,
                    Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                    "testing-payment-method",   // payment method
                    responseData,           // payment method data
                    "Bill A. Pacheco",          // payer name
                    "",                         // payer email
                    "");                        // payer phone
  paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function rejectShow(requestId) {
  const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                          createInstance(Ci.nsIGeneralResponseData);
  responseData.initData({});
  const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                          createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(requestId,
                    Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
                    "",                 // payment method
                    responseData,       // payment method data
                    "",                 // payer name
                    "",                 // payer email
                    "");                // payer phone
  paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function updateShow(requestId) {
  if (expectedUpdateAction == "updateaddress") {
    paymentSrv.changeShippingAddress(requestId, shippingAddress);
  } else if (expectedUpdateAction == "accept" || expectedUpdateAction == "error"){
    paymentSrv.changeShippingOption(requestId, "FastShipping");
  } else {
    emitTestFail("Unknown expected update action: " + expectedUpdateAction);
  }
}

function showRequest(requestId) {
  if (expectedShowAction == "accept") {
    acceptShow(requestId);
  } else if (expectedShowAction == "reject") {
    rejectShow(requestId);
  } else if (expectedShowAction == "update") {
    updateShow(requestId);
  } else {
    emitTestFail("Unknown expected show action: " + expectedShowAction);
  }
}

function abortRequest(requestId) {
  let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"].
                         createInstance(Ci.nsIPaymentAbortActionResponse);
  abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
  paymentSrv.respondPayment(abortResponse);
}

function completeRequest(requestId) {
  let payRequest = paymentSrv.getPaymentRequestById(requestId);
  if (expectedCompleteStatus) {
    if (payRequest.completeStatus == expectedCompleteStatus) {
      emitTestPass("request.completeStatus matches expectation of " +
                   expectedCompleteStatus);
    } else {
      emitTestFail("request.completeStatus incorrect. Expected " +
                   expectedCompleteStatus + ", got " + payRequest.completeStatus);
    }
  }
  let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                            createInstance(Ci.nsIPaymentCompleteActionResponse);
  completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
  paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function updateRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  if (expectedUpdateAction == "accept") {
    if (request.paymentDetails.error != "") {
      emitTestFail("updatedDetails should not have errors(" + request.paymentDetails.error + ").");
    }
    const shippingOptions = request.paymentDetails.shippingOptions;
    let shippingOption = shippingOptions.queryElementAt(0, Ci.nsIPaymentShippingOption);
    if (shippingOption.selected) {
      emitTestFail(shippingOption.label + " should not be selected.");
    }
    shippingOption = shippingOptions.queryElementAt(1, Ci.nsIPaymentShippingOption);
    if (!shippingOption.selected) {
      emitTestFail(shippingOption.label + " should be selected.");
    }
    acceptShow(requestId);
  } else if (expectedUpdateAction == "error") {
    if (request.paymentDetails.error != "Update with Error") {
      emitTestFail("details.error should be 'Update with Error', but got " + request.paymentDetails.error + ".");
    }
    rejectShow(requestId);
  } else if (expectedUpdateAction == "updateaddress") {
    if (request.paymentDetails.error != "") {
      emitTestFail("updatedDetails should not have errors(" + request.paymentDetails.error + ").");
    }
    expectedUpdateAction = "accept";
    paymentSrv.changeShippingOption(requestId, "FastShipping");
  } else {
    emitTestFail("Unknown expected update aciton: " + expectedUpdateAction);
  }
}

const DummyUIService = {
  showPayment: showRequest,
  abortPayment: abortRequest,
  completePayment: completeRequest,
  updatePayment: updateRequest,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

paymentSrv.setTestingUIService(DummyUIService.QueryInterface(Ci.nsIPaymentUIService));

function testShowResponseInit() {
  const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                              createInstance(Ci.nsIGeneralResponseData);
  try {
    showResponseData.initData(null);
    emitTestFail("nsIGeneralResponseData can not be initialized with null object.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("Expected 'NS_ERROR_FAILURE' when initializing nsIGeneralResponseData with null object, but got " + e.name + ".");
    }
    emitTestPass("Get expected result for initializing nsIGeneralResponseData with null object");
  }
  const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                          createInstance(Ci.nsIPaymentShowActionResponse);
  try {
    showResponse.init("test request id",
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "testing-payment-method",   // payment method
                      showResponseData,           // payment method data
                      "Bill A. Pacheco",          // payer name
                      "",                         // payer email
                      "");                        // payer phone
    emitTestPass("Get expected result for initializing response with accepted and empty data.");
  } catch (e) {
    emitTestFail("Unexpected error " + e.name + " when initializing response with accepted and empty data.");
  }

  try {
    showResponse.init("test request id",
                      Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
                      "testing-payment-method",
                      null,
                      "Bill A. Pacheco",
                      "",
                      "");
    emitTestPass("Get expected result for initializing response with rejected and null data.");
  } catch (e) {
    emitTestFail("Unexpected error " + e.name + " when initializing response with rejected and null data.");
  }

  try {
    showResponse.init("test request id",
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "testing-payment-method",
                      null,
                      "Bill A. Pacheco",
                      "",
                      "");
    emitTestFail("nsIPaymentShowActionResponse can not be initialized with accpeted and null data.");
  } catch (e) {
    if (e.name != "NS_ERROR_ILLEGAL_VALUE") {
      emitTestFail("Expected 'NS_ERROR_ILLEGAL_VALUE', but got " + e.name + ".");
    }
    emitTestPass("Get expected result for initializing response with accepted and null data.")
  }
  sendAsyncMessage("test-show-response-init-complete");
}

addMessageListener("set-simple-ui-service", function() {
  expectedCompleteStatus = null;
  expectedShowAction = "accept";
  expectedUpdateAction = "accept";
});

addMessageListener("set-normal-ui-service", function() {
  expectedCompleteStatus = null;
  expectedShowAction = "update";
  expectedUpdateAction = "updateaddress";
});

addMessageListener("set-reject-ui-service", function() {
  expectedCompleteStatus = null;
  expectedShowAction = "reject";
  expectedUpdateAction = "accept";
});

addMessageListener("set-update-with-ui-service", function() {
  expectedCompleteStatus = null;
  expectedShowAction = "update";
  expectedUpdateAction = "accept";
});

addMessageListener("set-update-with-error-ui-service", function() {
  expectedCompleteStatus = null;
  expectedShowAction = "update";
  expectedUpdateAction = "error";
});

addMessageListener("test-show-response-init", testShowResponseInit);

addMessageListener("set-complete-status-success", function() {
  expectedCompleteStatus = "success";
});

addMessageListener("set-complete-status-fail", function() {
  expectedCompleteStatus = "fail";
});

addMessageListener("set-complete-status-unknown", function() {
  expectedCompleteStatus = "unknown";
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
