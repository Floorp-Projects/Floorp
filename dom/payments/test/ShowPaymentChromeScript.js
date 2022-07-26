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
function emitTestPass(message) {
  sendAsyncMessage("test-pass", `${DummyUIService.testName}: ${message}`);
}

const shippingAddress = Cc[
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
shippingAddress.init(
  "USA", // country
  addressLine, // address line
  "CA", // region
  "CA", // region code
  "San Bruno", // city
  "Test locality", // dependent locality
  "94066", // postal code
  "123456", // sorting code
  "Testing Org", // organization
  "Bill A. Pacheco", // recipient
  "+1-434-441-3879"
); // phone

function acceptShow(requestId) {
  const responseData = Cc[
    "@mozilla.org/dom/payments/general-response-data;1"
  ].createInstance(Ci.nsIGeneralResponseData);
  responseData.initData({
    paymentToken: "6880281f-0df3-4b8e-916f-66575e2457c1",
  });
  let showResponse = Cc[
    "@mozilla.org/dom/payments/payment-show-action-response;1"
  ].createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(
    requestId,
    Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
    "testing-payment-method", // payment method
    responseData, // payment method data
    "Bill A. Pacheco", // payer name
    "", // payer email
    ""
  ); // payer phone
  paymentSrv.respondPayment(
    showResponse.QueryInterface(Ci.nsIPaymentActionResponse)
  );
}

function rejectShow(requestId) {
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

function updateShow(requestId) {
  if (DummyUIService.expectedUpdateAction == "updateaddress") {
    paymentSrv.changeShippingAddress(requestId, shippingAddress);
  } else if (
    DummyUIService.expectedUpdateAction == "accept" ||
    DummyUIService.expectedUpdateAction == "error"
  ) {
    paymentSrv.changeShippingOption(requestId, "FastShipping");
  } else {
    emitTestFail(
      "Unknown expected update action: " + DummyUIService.expectedUpdateAction
    );
  }
}

function showRequest(requestId) {
  const request = paymentSrv.getPaymentRequestById(requestId);
  if (request.completeStatus == "initial") {
    return;
  }
  if (DummyUIService.expectedShowAction == "accept") {
    acceptShow(requestId);
  } else if (DummyUIService.expectedShowAction == "reject") {
    rejectShow(requestId);
  } else if (DummyUIService.expectedShowAction == "update") {
    updateShow(requestId);
  } else {
    emitTestFail(
      "Unknown expected show action: " + DummyUIService.expectedShowAction
    );
  }
}

function abortRequest(requestId) {
  let abortResponse = Cc[
    "@mozilla.org/dom/payments/payment-abort-action-response;1"
  ].createInstance(Ci.nsIPaymentAbortActionResponse);
  abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
  paymentSrv.respondPayment(abortResponse);
}

function completeRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  if (DummyUIService.expectedCompleteStatus) {
    if (request.completeStatus == DummyUIService.expectedCompleteStatus) {
      emitTestPass(
        "request.completeStatus matches expectation of " +
          DummyUIService.expectedCompleteStatus
      );
    } else {
      emitTestFail(
        "request.completeStatus incorrect. Expected " +
          DummyUIService.expectedCompleteStatus +
          ", got " +
          request.completeStatus
      );
    }
  }
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

function updateRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  if (request.completeStatus !== "") {
    emitTestFail(
      "request.completeStatus should be empty, but got '" +
        request.completeStatus +
        "'."
    );
  }
  if (DummyUIService.expectedUpdateAction == "accept") {
    if (request.paymentDetails.error != "") {
      emitTestFail(
        "updatedDetails should not have errors(" +
          request.paymentDetails.error +
          ")."
      );
    }
    const shippingOptions = request.paymentDetails.shippingOptions;
    let shippingOption = shippingOptions.queryElementAt(
      0,
      Ci.nsIPaymentShippingOption
    );
    if (shippingOption.selected) {
      emitTestFail(shippingOption.label + " should not be selected.");
    }
    shippingOption = shippingOptions.queryElementAt(
      1,
      Ci.nsIPaymentShippingOption
    );
    if (!shippingOption.selected) {
      emitTestFail(shippingOption.label + " should be selected.");
    }
    acceptShow(requestId);
  } else if (DummyUIService.expectedUpdateAction == "error") {
    if (request.paymentDetails.error != "Update with Error") {
      emitTestFail(
        "details.error should be 'Update with Error', but got " +
          request.paymentDetails.error +
          "."
      );
    }
    rejectShow(requestId);
  } else if (DummyUIService.expectedUpdateAction == "updateaddress") {
    if (request.paymentDetails.error != "") {
      emitTestFail(
        "updatedDetails should not have errors(" +
          request.paymentDetails.error +
          ")."
      );
    }
    DummyUIService.expectedUpdateAction = "accept";
    paymentSrv.changeShippingOption(requestId, "FastShipping");
  } else {
    emitTestFail(
      "Unknown expected update aciton: " + DummyUIService.expectedUpdateAction
    );
  }
}

const DummyUIService = {
  testName: "",
  expectedCompleteStatus: null,
  expectedShowAction: "accept",
  expectedUpdateAction: "accept",
  showPayment: showRequest,
  abortPayment: abortRequest,
  completePayment: completeRequest,
  updatePayment: updateRequest,
  closePayment(requestId) {},
  QueryInterface: ChromeUtils.generateQI(["nsIPaymentUIService"]),
};

paymentSrv.setTestingUIService(
  DummyUIService.QueryInterface(Ci.nsIPaymentUIService)
);

function testShowResponseInit() {
  const showResponseData = Cc[
    "@mozilla.org/dom/payments/general-response-data;1"
  ].createInstance(Ci.nsIGeneralResponseData);
  try {
    showResponseData.initData(null);
    emitTestFail(
      "nsIGeneralResponseData can not be initialized with null object."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail(
        "Expected 'NS_ERROR_FAILURE' when initializing nsIGeneralResponseData with null object, but got " +
          e.name +
          "."
      );
    }
    emitTestPass(
      "Get expected result for initializing nsIGeneralResponseData with null object"
    );
  }
  const showResponse = Cc[
    "@mozilla.org/dom/payments/payment-show-action-response;1"
  ].createInstance(Ci.nsIPaymentShowActionResponse);
  try {
    showResponse.init(
      "test request id",
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "testing-payment-method", // payment method
      showResponseData, // payment method data
      "Bill A. Pacheco", // payer name
      "", // payer email
      ""
    ); // payer phone
    emitTestPass(
      "Get expected result for initializing response with accepted and empty data."
    );
  } catch (e) {
    emitTestFail(
      "Unexpected error " +
        e.name +
        " when initializing response with accepted and empty data."
    );
  }

  try {
    showResponse.init(
      "test request id",
      Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
      "testing-payment-method",
      null,
      "Bill A. Pacheco",
      "",
      ""
    );
    emitTestPass(
      "Get expected result for initializing response with rejected and null data."
    );
  } catch (e) {
    emitTestFail(
      "Unexpected error " +
        e.name +
        " when initializing response with rejected and null data."
    );
  }

  try {
    showResponse.init(
      "test request id",
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "testing-payment-method",
      null,
      "Bill A. Pacheco",
      "",
      ""
    );
    emitTestFail(
      "nsIPaymentShowActionResponse can not be initialized with accpeted and null data."
    );
  } catch (e) {
    if (e.name != "NS_ERROR_ILLEGAL_VALUE") {
      emitTestFail(
        "Expected 'NS_ERROR_ILLEGAL_VALUE', but got " + e.name + "."
      );
    }
    emitTestPass(
      "Get expected result for initializing response with accepted and null data."
    );
  }
  sendAsyncMessage("test-show-response-init-complete");
}

addMessageListener("set-simple-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.expectedCompleteStatus = null;
  DummyUIService.expectedShowAction = "accept";
  DummyUIService.expectedUpdateAction = "accept";
  sendAsyncMessage("set-simple-ui-service-complete");
});

addMessageListener("set-normal-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.expectedCompleteStatus = null;
  DummyUIService.expectedShowAction = "update";
  DummyUIService.expectedUpdateAction = "updateaddress";
  sendAsyncMessage("set-normal-ui-service-complete");
});

addMessageListener("set-reject-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.expectedCompleteStatus = null;
  DummyUIService.expectedShowAction = "reject";
  DummyUIService.expectedUpdateAction = "error";
  sendAsyncMessage("set-reject-ui-service-complete");
});

addMessageListener("set-update-with-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.expectedCompleteStatus = null;
  DummyUIService.expectedShowAction = "update";
  DummyUIService.expectedUpdateAction = "accept";
  sendAsyncMessage("set-update-with-ui-service-complete");
});

addMessageListener("set-update-with-error-ui-service", function(testName) {
  DummyUIService.testName = testName;
  DummyUIService.expectedCompleteStatus = null;
  DummyUIService.expectedShowAction = "update";
  DummyUIService.expectedUpdateAction = "error";
  sendAsyncMessage("set-update-with-error-ui-service-complete");
});

addMessageListener("test-show-response-init", testShowResponseInit);

addMessageListener("set-complete-status-success", function() {
  DummyUIService.expectedCompleteStatus = "success";
  sendAsyncMessage("set-complete-status-success-complete");
});

addMessageListener("set-complete-status-fail", function() {
  DummyUIService.expectedCompleteStatus = "fail";
  sendAsyncMessage("set-complete-status-fail-complete");
});

addMessageListener("set-complete-status-unknown", function() {
  DummyUIService.expectedCompleteStatus = "unknown";
  sendAsyncMessage("set-complete-status-unknown-complete");
});

addMessageListener("teardown", function() {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
