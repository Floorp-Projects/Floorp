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

const TestingUIService = {
  showPayment(requestId, name = "", email = "", phone = "") {
    const showResponseData = Cc[
      "@mozilla.org/dom/payments/general-response-data;1"
    ].createInstance(Ci.nsIGeneralResponseData);
    showResponseData.initData({});
    const showResponse = Cc[
      "@mozilla.org/dom/payments/payment-show-action-response;1"
    ].createInstance(Ci.nsIPaymentShowActionResponse);
    showResponse.init(
      requestId,
      Ci.nsIPaymentActionResponse.PAYMENT_ACCEPTED,
      "testing-payment-method", // payment method
      showResponseData, // payment method data
      name,
      email,
      phone
    );
    paymentSrv.respondPayment(
      showResponse.QueryInterface(Ci.nsIPaymentActionResponse)
    );
  },
  // .retry({ payer }) and .updateWith({payerErrors}) both get routed here:
  updatePayment(requestId) {
    // Let's echo what was sent in by the error...
    const request = paymentSrv.getPaymentRequestById(requestId);
    const { name, email, phone } = request.paymentDetails.payerErrors;
    const { error } = request.paymentDetails;
    // Let's use the .error as the switch
    switch (error) {
      case "retry-fire-payerdetaichangeevent": {
        paymentSrv.changePayerDetail(requestId, name, email, phone);
        break;
      }
      case "update-with": {
        this.showPayment(requestId, name, email, phone);
        break;
      }
      default:
        const msg = `Expect details.error value: '${error}'`;
        sendAsyncMessage("test-fail", msg);
    }
  },
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
