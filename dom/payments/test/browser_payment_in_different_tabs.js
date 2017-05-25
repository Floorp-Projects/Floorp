"use strict";

// kTestRoot is from head.js
const kTestPage = kTestRoot + "simple_payment_request.html";

registerCleanupFunction(cleanup);

add_task(function*() {
  Services.prefs.setBoolPref("dom.payments.request.enabled", true);
  yield BrowserTestUtils.withNewTab(kTestPage,
    function*(browser) {
      yield BrowserTestUtils.withNewTab(kTestPage,
        function*(browser) {
          const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
          ok(paymentSrv, "Fail to get PaymentRequestService.");

          const paymentEnum = paymentSrv.enumerate();
          ok(paymentEnum.hasMoreElements(), "PaymentRequestService should have at least one payment request.");
          let tabIds = [];
          while (paymentEnum.hasMoreElements()) {
            let payment = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
            ok(payment, "Fail to get existing payment request.");
            checkSimplePayment(payment);
            tabIds.push(payment.tabId);
          }
          is(tabIds.length, 2, "TabId array length should be 2.");
          ok(tabIds[0] != tabIds[1], "TabIds should be different.");
          Services.prefs.setBoolPref("dom.payments.request.enabled", false);
        }
      );
    }
  );
});
