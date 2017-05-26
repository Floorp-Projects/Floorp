"use strict";

// kTestRoot is from head.js
const kTestPage = kTestRoot + "multiple_payment_request.html";

registerCleanupFunction(cleanup);

add_task(function*() {
  Services.prefs.setBoolPref("dom.payments.request.enabled", true);
  yield BrowserTestUtils.withNewTab(kTestPage,
    function*(browser) {

      const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
      ok(paymentSrv, "Fail to get PaymentRequestService.");

      const paymentEnum = paymentSrv.enumerate();
      ok(paymentEnum.hasMoreElements(), "PaymentRequestService should have at least one payment request.");
      while (paymentEnum.hasMoreElements()) {
        let payment = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
        ok(payment, "Fail to get existing payment request.");
        if (payment.paymentDetails.id == "complex details") {
          checkComplexPayment(payment);
        } else if (payment.paymentDetails.id == "simple details") {
          checkSimplePayment(payment);
        } else {
          ok(false, "Unknown payment.");
        }
      }
      Services.prefs.setBoolPref("dom.payments.request.enabled", false);
    }
  );
});
