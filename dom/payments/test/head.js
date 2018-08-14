const kTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content",
                                                      "https://example.com");

function checkSimplePayment(aSimplePayment) {
  // checking the passed PaymentMethods parameter
  is(aSimplePayment.paymentMethods.length, 1, "paymentMethods' length should be 1.");

  const methodData = aSimplePayment.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  ok(methodData, "Fail to get payment methodData.");
  is(methodData.supportedMethods, "basic-card", "supported method should be 'basic-card'.");
  ok(!methodData.data, "methodData.data should not exist.");

  // checking the passed PaymentDetails parameter
  const details = aSimplePayment.paymentDetails;
  is(details.id, "simple details", "details.id should be 'simple details'.");
  is(details.totalItem.label, "Donation", "total item's label should be 'Donation'.");
  is(details.totalItem.amount.currency, "USD", "total item's currency should be 'USD'.");
  is(details.totalItem.amount.value, "55.00", "total item's value should be '55.00'.");

  ok(!details.displayItems, "details.displayItems should be undefined.");
  ok(!details.modifiers, "details.modifiers should be undefined.");
  ok(!details.shippingOptions, "details.shippingOptions should be undefined.");

  // checking the default generated PaymentOptions parameter
  const paymentOptions = aSimplePayment.paymentOptions;
  ok(!paymentOptions.requestPayerName, "payerName option should be false");
  ok(!paymentOptions.requestPayerEmail, "payerEmail option should be false");
  ok(!paymentOptions.requestPayerPhone, "payerPhone option should be false");
  ok(!paymentOptions.requestShipping, "requestShipping option should be false");
  is(paymentOptions.shippingType, "shipping", "shippingType option should be 'shipping'");
}

function checkDupShippingOptionsPayment(aPayment) {
  // checking the passed PaymentMethods parameter
  is(aPayment.paymentMethods.length, 1, "paymentMethods' length should be 1.");

  const methodData = aPayment.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  ok(methodData, "Fail to get payment methodData.");
  is(methodData.supportedMethods, "basic-card", "methodData.supportedMethod name should be 'basic-card'.");
  ok(!methodData.data, "methodData.data should not exist.");

  // checking the passed PaymentDetails parameter
  const details = aPayment.paymentDetails;
  is(details.id, "duplicate shipping options details", "details.id should be 'duplicate shipping options details'.");
  is(details.totalItem.label, "Donation", "total item's label should be 'Donation'.");
  is(details.totalItem.amount.currency, "USD", "total item's currency should be 'USD'.");
  is(details.totalItem.amount.value, "55.00", "total item's value should be '55.00'.");

  const shippingOptions = details.shippingOptions;
  is(shippingOptions.length, 0, "shippingOptions' length should be 0.");

  // checking the passed PaymentOptions parameter
  const paymentOptions = aPayment.paymentOptions;
  ok(paymentOptions.requestPayerName, "payerName option should be true");
  ok(paymentOptions.requestPayerEmail, "payerEmail option should be true");
  ok(paymentOptions.requestPayerPhone, "payerPhone option should be true");
  ok(paymentOptions.requestShipping, "requestShipping option should be true");
  is(paymentOptions.shippingType, "shipping", "shippingType option should be 'shipping'");
}
