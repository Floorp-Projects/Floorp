/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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
shippingAddress.init("",  // country
                     addressLine, // address line
                     "",  // region
                     "",  // city
                     "",  // dependent locality
                     "",  // postal code
                     "",  // sorting code
                     "",  // language code
                     "",  // organization
                     "",  // recipient
                     ""); // phone

const NormalUIService = {
  shippingOptionChanged: false,
  showPayment: function(requestId) {
    paymentSrv.changeShippingAddress(requestId, shippingAddress);
  },
  abortPayment: function(requestId) {
  },
  completePayment: function(requestId) {
    let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                           createInstance(Ci.nsIPaymentCompleteActionResponse);
    completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
    paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  updatePayment: function(requestId) {
    let showResponse = null;
    let payRequest = paymentSrv.getPaymentRequestById(requestId);

    const shippingOptions = payRequest.paymentDetails.shippingOptions;
    if (shippingOptions.length != 0) {
      emitTestFail("Wrong length for shippingOptions.");
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
                      "",                         // payer name
                      "",                         // payer email
                      "");                        // payer phone
    paymentSrv.respondPayment(showResponse.QueryInterface(Ci.nsIPaymentActionResponse));
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

addMessageListener("set-normal-ui-service", function() {
  paymentSrv.setTestingUIService(NormalUIService.QueryInterface(Ci.nsIPaymentUIService));
});

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
