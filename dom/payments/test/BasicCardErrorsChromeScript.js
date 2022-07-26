/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-script */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const paymentSrv = Cc[
  "@mozilla.org/dom/payments/payment-request-service;1"
].getService(Ci.nsIPaymentRequestService);

const defaultCard = {
  cardholderName: "",
  cardNumber: "4111111111111111",
  expiryMonth: "",
  expiryYear: "",
  cardSecurityCode: "",
  billingAddress: null,
};

function makeBillingAddress() {
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
  const addressArgs = [
    "USA", // country
    addressLine, // address line
    "CA", // region
    "CA", // regionCode
    "San Bruno", // city
    "", // dependent locality
    "94066", // postal code
    "123456", // sorting code
    "", // organization
    "Bill A. Pacheco", // recipient
    "+14344413879", // phone
  ];
  billingAddress.init(...addressArgs);
  return billingAddress;
}

function makeBasicCardResponse(details) {
  const basicCardResponseData = Cc[
    "@mozilla.org/dom/payments/basiccard-response-data;1"
  ].createInstance(Ci.nsIBasicCardResponseData);
  const {
    cardholderName,
    cardNumber,
    expiryMonth,
    expiryYear,
    cardSecurityCode,
    billingAddress,
  } = details;

  const address =
    billingAddress !== undefined ? billingAddress : makeBillingAddress();

  basicCardResponseData.initData(
    cardholderName,
    cardNumber,
    expiryMonth,
    expiryYear,
    cardSecurityCode,
    address
  );

  return basicCardResponseData;
}

const TestingUIService = {
  showPayment(requestId, details = { ...defaultCard }) {
    const showResponse = Cc[
      "@mozilla.org/dom/payments/payment-show-action-response;1"
    ].createInstance(Ci.nsIPaymentShowActionResponse);

    showResponse.init(
      requestId,
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "basic-card", // payment method
      makeBasicCardResponse(details),
      "Person name",
      "Person email",
      "Person phone"
    );

    paymentSrv.respondPayment(
      showResponse.QueryInterface(Ci.nsIPaymentActionResponse)
    );
  },
  // Handles response.retry({ paymentMethod }):
  updatePayment(requestId) {
    // Let's echo what was sent in by the error...
    const request = paymentSrv.getPaymentRequestById(requestId);
    this.showPayment(requestId, request.paymentDetails.paymentMethodErrors);
  },
  // Handles response.complete()
  completePayment(requestId) {
    const request = paymentSrv.getPaymentRequestById(requestId);
    const completeResponse = Cc[
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
  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIPaymentUIService"]);
  },
};

paymentSrv.setTestingUIService(
  TestingUIService.QueryInterface(Ci.nsIPaymentUIService)
);

addMessageListener("teardown", () => {
  paymentSrv.setTestingUIService(null);
  sendAsyncMessage("teardown-complete");
});
