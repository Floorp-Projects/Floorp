"use strict";

// kTestRoot is from head.js
const kTestPage = kTestRoot + "simple_payment_request.html";

registerCleanupFunction(cleanup);

add_task(async function() {
  Services.prefs.setBoolPref("dom.payments.request.enabled", true);
  await BrowserTestUtils.withNewTab(kTestPage,
    function(browser) {

      const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
      ok(paymentSrv, "Fail to get PaymentRequestService.");

      const paymentEnum = paymentSrv.enumerate();
      ok(paymentEnum.hasMoreElements(), "PaymentRequestService should have at least one payment request.");
      while (paymentEnum.hasMoreElements()) {
        let payment = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
        ok(payment, "Fail to get existing payment request.");
        checkSimplePayment(payment);
      }
      Services.prefs.setBoolPref("dom.payments.request.enabled", false);
    }
  );
});
