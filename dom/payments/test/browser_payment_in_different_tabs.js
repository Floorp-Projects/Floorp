"use strict";

// kTestRoot is from head.js
const kTestPage = kTestRoot + "simple_payment_request.html";
const TABS_TO_OPEN = 5;
add_task(async () => {
  Services.prefs.setBoolPref("dom.payments.request.enabled", true);
  const tabs = [];
  const options = {
    gBrowser: Services.wm.getMostRecentWindow("navigator:browser").gBrowser,
    url: kTestPage,
  };
  for (let i = 0; i < TABS_TO_OPEN; i++) {
    const tab = await BrowserTestUtils.openNewForegroundTab(options);
    tabs.push(tab);
  }
  const paymentSrv = Cc[
    "@mozilla.org/dom/payments/payment-request-service;1"
  ].getService(Ci.nsIPaymentRequestService);
  const paymentEnum = paymentSrv.enumerate();
  ok(
    paymentEnum.hasMoreElements(),
    "PaymentRequestService should have at least one payment request."
  );
  const payments = new Set();
  for (let payment of paymentEnum) {
    ok(payment, "Fail to get existing payment request.");
    checkSimplePayment(payment);
    payments.add(payment);
  }
  is(payments.size, TABS_TO_OPEN, `Should be ${TABS_TO_OPEN} unique objects.`);
  for (const tab of tabs) {
    await TestUtils.waitForTick();
    BrowserTestUtils.removeTab(tab);
  }
  Services.prefs.setBoolPref("dom.payments.request.enabled", false);
});
