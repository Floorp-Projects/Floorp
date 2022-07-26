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

const InvalidDetailsUIService = {
  showPayment(requestId) {
    paymentSrv.changeShippingOption(requestId, "");
  },
  abortPayment(requestId) {
    const abortResponse = Cc[
      "@mozilla.org/dom/payments/payment-abort-action-response;1"
    ].createInstance(Ci.nsIPaymentAbortActionResponse);
    abortResponse.init(requestId, Ci.nsIPaymentActionResponse.ABORT_SUCCEEDED);
    paymentSrv.respondPayment(
      abortResponse.QueryInterface(Ci.nsIPaymentActionResponse)
    );
  },
  completePayment(requestId) {},
  updatePayment(requestId) {},
  closePayment(requestId) {},
  QueryInterface: ChromeUtils.generateQI(["nsIPaymentUIService"]),
};

function checkLowerCaseCurrency() {
  const paymentEnum = paymentSrv.enumerate();
  if (!paymentEnum.hasMoreElements()) {
    const msg =
      "PaymentRequestService should have at least one payment request.";
    sendAsyncMessage("test-fail", msg);
  }
  for (let payRequest of paymentEnum) {
    if (!payRequest) {
      sendAsyncMessage("test-fail", "Fail to get existing payment request.");
      break;
    }
    const { currency } = payRequest.paymentDetails.totalItem.amount;
    if (currency != "USD") {
      const msg =
        "Currency of PaymentItem total should be 'USD', but got ${currency}";
      sendAsyncMessage("check-complete");
    }
  }
  paymentSrv.cleanup();
  sendAsyncMessage("check-complete");
}

addMessageListener("check-lower-case-currency", checkLowerCaseCurrency);

addMessageListener("set-update-with-invalid-details-ui-service", () => {
  paymentSrv.setTestingUIService(
    InvalidDetailsUIService.QueryInterface(Ci.nsIPaymentUIService)
  );
});

addMessageListener("teardown", () => sendAsyncMessage("teardown-complete"));
