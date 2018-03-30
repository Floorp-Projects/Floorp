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

const TestingUIService = {
  showPayment: function(requestId) {
    paymentSrv.changeShippingAddress(requestId, shippingAddress);
  },
  abortPayment: function(requestId) {
  },
  completePayment: function(requestId) {
    let request = paymentSrv.getPaymentRequestById(requestId);
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                           createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  updatePayment: function(requestId) {
    let request = paymentSrv.getPaymentRequestById(requestId);
    if (request.shippingOptions != null) {
      emitTestFail("request.shippingOptions should be null");
    } else {
      emitTestPass("request.shippingOptions should be null");
    }
    const showResponseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                                createInstance(Ci.nsIGeneralResponseData);
    showResponseData.initData({});
    const showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                            createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(requestId,
                      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                      "testing-payment-method",   // payment method
                      showResponseData,           // payment method data
                      "Bill A. Pacheco",          // payer name
                      "",                         // payer email
                      "");                        // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPaymentUIService]),
};

addMessageListener("set-checking-shipping-options-ui-service", function() {
  paymentSrv.setTestingUIService(TestingUIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
