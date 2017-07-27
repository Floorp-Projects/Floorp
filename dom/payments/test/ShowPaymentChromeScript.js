/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
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
                     "",                 // dependent locality
                     "94066",            // postal code
                     "123456",           // sorting code
                     "en",               // language code
                     "",                 // organization
                     "Bill A. Pacheco",  // recipient
                     "+1-434-441-3879"); // phone

const NormalUIService = {
  shippingOptionChanged: false,
  canMakePayment: function(requestId) {
    return null;
  },
  showPayment: function(requestId) {
    paymentSrv.changeShippingAddress(requestId, shippingAddress);
    return null;
  },
  abortPayment: function(requestId) {
    return null;
  },
  completePayment: function(requestId) {
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                           createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
    return completeResponse;
  },
  updatePayment: function(requestId) {
    let showResponse = null;
    let payRequest = paymentSrv.getPaymentRequestById(requestId);
    if (payRequest.paymentDetails.error != "") {
      emitTestFail("updatedDetails should not have errors(" + payRequest.paymentDetails.error + ").");
    }
    if (!this.shippingOptionChanged) {
      paymentSrv.changeShippingOption(requestId, "FastShipping");
      this.shippingOptionChanged = true;
    } else {
      const shippingOptions = payRequest.paymentDetails.shippingOptions;
      let shippingOption = shippingOptions.queryElementAt(0, Ci.nsIPaymentShippingOption);
      if (shippingOption.selected) {
        emitTestFail(shippingOption.label + " should not be selected.");
      }
      shippingOption = shippingOptions.queryElementAt(1, Ci.nsIPaymentShippingOption);
      if (!shippingOption.selected) {
        emitTestFail(shippingOption.label + " should be selected.");
      }

      const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                                  createInstance(Ci.nsIGeneralResponseData);

      try {
        showResponseData.initData({ paymentToken: "6880281f-0df3-4b8e-916f-66575e2457c1",});
      } catch (e) {
        emitTestFail("Fail to initialize response data with { paymentToken: \"6880281f-0df3-4b8e-916f-66575e2457c1\",}");
      }

      showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                     createInstance(Ci.nsIPaymentShowActionResponse);
      showResponse.init(requestId,
                        Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                        "testing-payment-method",   // payment method
                        showResponseData,           // payment method data
                        "Bill A. Pacheco",          // payer name
                        "",                         // payer email
                        "");                        // payer phone
    }
    return showResponse;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
};

const RejectUIService = {
  canMakePayment: function(requestId) {
    return null;
  },
  showPayment: function(requestId) {
    const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                            createInstance(Ci.nsIGeneralResponseData);

    try {
      responseData.initData({});
    } catch (e) {
      emitTestFail("Fail to initialize response data with empty object.");
    }
    const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                            createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
                      "",                 // payment method
                      responseData,       // payment method data
                      "",                 // payer name
                      "",                 // payer email
                      "");                // payer phone

    return showResponse;
  },
  abortPayment: function(requestId) {
    return null;
  },
  completePayment: function(requestId) {
    return null;
  },
  updatePayment: function(requestId) {
    return null;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
};

const ErrorUIService = {
  canMakePayment: function(requestId) {
    return null;
  },
  showPayment: function(requestId) {
    paymentSrv.changeShippingOption(requestId, "");
    return null;
  },
  abortPayment: function(requestId) {
    return null;
  },
  completePayment: function(requestId) {
    return null;
  },
  updatePayment: function(requestId) {
    let payRequest = paymentSrv.getPaymentRequestById(requestId);
    if (!payRequest) {
      emitTestFail("Fail to get existing payment request.");
    }
    if (payRequest.paymentDetails.error != "Update with Error") {
      emitTestFail("details.error should be 'Update with Error', but got " + payRequest.paymentDetails.error + ".");
    }
    const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                            createInstance(Ci.nsIGeneralResponseData);
    try {
      responseData.initData({});
    } catch (e) {
      emitTestFail("Fail to initialize response data with empty object.");
    }
    const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                            createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_REJECTED,
                      "",                 // payment method
                      responseData,       // payment method data
                      "",                 // payer name
                      "",                 // payer email
                      "");                // payer phone

    return showResponse;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),

};

function testNullDetailsResponseHandler() {
  const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                              createInstance(Ci.nsIGeneralResponseData);
  try {
    showResponseData.initData(null);
    emitTestFail("nsIGeneralResponseData can not be initialized with null object.");
  } catch (e) {
    if (e.name != "NS_ERROR_FAILURE") {
      emitTestFail("Expected 'NS_ERROR_FAILURE' when initializing nsIGeneralResponseData with null object, but got " + e.name + ".");
    }
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
  } catch (e) {
    emitTestFail("Unexpected error " + e.name + " when initializing nsIPaymentShowActionResponse.");
  }
  sendAsyncMessage("test-null-details-response-complete");
}

addMessageListener("set-normal-ui-service", function() {
  paymentSrv.setTestingUIService(NormalUIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("set-reject-ui-service", function() {
  paymentSrv.setTestingUIService(RejectUIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("set-update-with-error-ui-service", function() {
  paymentSrv.setTestingUIService(ErrorUIService.QueryInterface(Ci.nsIPaymentUIService));
})

addMessageListener("test-null-details-response", testNullDetailsResponseHandler);


addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
