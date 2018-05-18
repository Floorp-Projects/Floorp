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

function acceptShow(requestId) {
  const responseData = Cc["@mozilla.org/dom/payments/general-response-data;1"].
                          createInstance(Ci.nsIGeneralResponseData);
  responseData.initData({ paymentToken: "6880281f-0df3-4b8e-916f-66575e2457c1",});
  let showResponse = Cc["@mozilla.org/dom/payments/payment-show-action-response;1"].
                        createInstance(Ci.nsIPaymentShowActionResponse);
  showResponse.init(requestId,
                    Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
                    "testing-payment-method",   // payment method
                    responseData,               // payment method data
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
  paymentSrv.changeShippingAddress(requestId, shippingAddress);
}

function showRequest(requestId) {
  updateShow(requestId);
}

function abortRequest(requestId) {
  let abortResponse = Cc["@mozilla.org/dom/payments/payment-abort-action-response;1"].
                         createInstance(Ci.nsIPaymentAbortActionResponse);
  abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
  paymentSrv.respondPayment(abortResponse);
}

function completeRequest(requestId) {
  let payRequest = paymentSrv.getPaymentRequestById(requestId);
  let completeResponse = Cc["@mozilla.org/dom/payments/payment-complete-action-response;1"].
                            createInstance(Ci.nsIPaymentCompleteActionResponse);
  completeResponse.init(requestId, Ci.nsIPaymentActionResponse.COMPLETE_SUCCEEDED);
  paymentSrv.respondPayment(completeResponse.QueryInterface(Ci.nsIPaymentActionResponse));
}

function checkAddressErrors(errors) {
  if (!errors) {
    emitTestFail("Expect non-null shippingAddressErrors, but got null.");
  }
  if (errors.addressLine != "addressLine error") {
    emitTestFail("Expect shippingAddressErrors.addressLine as 'addressLine error', but got" +
                  errors.addressLine);
  }
  if (errors.city != "city error") {
    emitTestFail("Expect shippingAddressErrors.city as 'city error', but got" +
                  errors.city);
  }
  if (errors.dependentLocality != "dependentLocality error") {
    emitTestFail("Expect shippingAddressErrors.dependentLocality as 'dependentLocality error', but got" +
                  errors.dependentLocality);
  }
  if (errors.languageCode != "languageCode error") {
    emitTestFail("Expect shippingAddressErrors.languageCode as 'languageCode error', but got" +
                  errors.languageCode);
  }
  if (errors.organization != "organization error") {
    emitTestFail("Expect shippingAddressErrors.organization as 'organization error', but got" +
                  errors.organization);
  }
  if (errors.phone != "phone error") {
    emitTestFail("Expect shippingAddressErrors.phone as 'phone error', but got" +
                  errors.phone);
  }
  if (errors.postalCode != "postalCode error") {
    emitTestFail("Expect shippingAddressErrors.postalCode as 'postalCode error', but got" +
                  errors.postalCode);
  }
  if (errors.recipient != "recipient error") {
    emitTestFail("Expect shippingAddressErrors.recipient as 'recipient error', but got" +
                  errors.recipient);
  }
  if (errors.region != "region error") {
    emitTestFail("Expect shippingAddressErrors.region as 'region error', but got" +
                  errors.region);
  }
  if (errors.sortingCode != "sortingCode error") {
    emitTestFail("Expect shippingAddressErrors.sortingCode as 'sortingCode error', but got" +
                  errors.sortingCode);
  }
}

function updateRequest(requestId) {
  let request = paymentSrv.getPaymentRequestById(requestId);
  const addressErrors = request.paymentDetails.shippingAddressErrors;
  const payerErrors = request.paymentDetails.payerErrors;
  checkAddressErrors(addressErrors);
  rejectShow(requestId);
}

const DummyUIService = {
  showPayment: showRequest,
  abortPayment: abortRequest,
  completePayment: completeRequest,
  updatePayment: updateRequest,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPaymentUIService]),
};

paymentSrv.setTestingUIService(DummyUIService.QueryInterface(Ci.nsIPaymentUIService));

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage('teardown-complete');
});
