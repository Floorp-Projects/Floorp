const kTestRoot = getRootDirectory(gTestPath).replace("chrome://mochitests/content",
                                                      "https://example.com");

function checkSimplePayment(aSimplePayment) {
  // checking the passed PaymentMethods parameter
  is(aSimplePayment.paymentMethods.length, 1, "paymentMethods' length should be 1.");

  const methodData = aSimplePayment.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  ok(methodData, "Fail to get payment methodData.");
  is(methodData.supportedMethods, "MyPay", "supported method should be 'MyPay'.");
  is(methodData.data, "", "method data should be empty");

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

function checkComplexPayment(aPayment) {
  // checking the passed PaymentMethods parameter
  is(aPayment.paymentMethods.length, 1, "paymentMethods' length should be 1.");

  const methodData = aPayment.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  ok(methodData, "Fail to get payment methodData.");
  is(methodData.supportedMethods, "MyPay", "supported method should be 'MyPay'.");
  is(methodData.data, "", "method data should be empty");

  // checking the passed PaymentDetails parameter
  const details = aPayment.paymentDetails;
  is(details.id, "complex details", "details.id should be 'complex details'.");
  is(details.totalItem.label, "Donation", "total item's label should be 'Donation'.");
  is(details.totalItem.amount.currency, "USD", "total item's currency should be 'USD'.");
  is(details.totalItem.amount.value, "55.00", "total item's value should be '55.00'.");

  const displayItems = details.displayItems;
  is(displayItems.length, 2, "displayItems' length should be 2.");
  let item = displayItems.queryElementAt(0, Ci.nsIPaymentItem);
  is(item.label, "Original donation amount", "1st display item's label should be 'Original donation amount'.");
  is(item.amount.currency, "USD", "1st display item's currency should be 'USD'.");
  is(item.amount.value, "-65.00", "1st display item's value should be '-65.00'.");
  item = displayItems.queryElementAt(1, Ci.nsIPaymentItem);
  is(item.label, "Friends and family discount", "2nd display item's label should be 'Friends and family discount'.");
  is(item.amount.currency, "USD", "2nd display item's currency should be 'USD'.");
  is(item.amount.value, "10.00", "2nd display item's value should be '10.00'.");

  const modifiers = details.modifiers;
  is(modifiers.length, 1, "modifiers' length should be 1.");

  const modifier = modifiers.queryElementAt(0, Ci.nsIPaymentDetailsModifier);
  is(modifier.supportedMethods, "MyPay", "modifier's supported method name should be 'MyPay'.");
  is(modifier.total.label, "Discounted donation", "modifier's total label should be 'Discounted donation'.");
  is(modifier.total.amount.currency, "USD", "modifier's total currency should be 'USD'.");
  is(modifier.total.amount.value, "45.00", "modifier's total value should be '45.00'.");

  const additionalItems = modifier.additionalDisplayItems;
  is(additionalItems.length, "1", "additionalDisplayItems' length should be 1.");
  const additionalItem = additionalItems.queryElementAt(0, Ci.nsIPaymentItem);
  is(additionalItem.label, "MyPay discount", "additional item's label should be 'MyPay discount'.");
  is(additionalItem.amount.currency, "USD", "additional item's currency should be 'USD'.");
  is(additionalItem.amount.value, "-10.00", "additional item's value should be '-10.00'.");
  is(modifier.data, "{\"discountProgramParticipantId\":\"86328764873265\"}",
     "modifier's data should be '{\"discountProgramParticipantId\":\"86328764873265\"}'.");

  const shippingOptions = details.shippingOptions;
  is(shippingOptions.length, 2, "shippingOptions' length should be 2.");

  let shippingOption = shippingOptions.queryElementAt(0, Ci.nsIPaymentShippingOption);
  is(shippingOption.id, "NormalShipping", "1st shippingOption's id should be 'NoramlShpping'.");
  is(shippingOption.label, "NormalShipping", "1st shippingOption's lable should be 'NormalShipping'.");
  is(shippingOption.amount.currency, "USD", "1st shippingOption's amount currency should be 'USD'.");
  is(shippingOption.amount.value, "10.00", "1st shippingOption's amount value should be '10.00'.");
  ok(shippingOption.selected, "1st shippingOption should be selected.");

  shippingOption = shippingOptions.queryElementAt(1, Ci.nsIPaymentShippingOption);
  is(shippingOption.id, "FastShipping", "2nd shippingOption's id should be 'FastShpping'.");
  is(shippingOption.label, "FastShipping", "2nd shippingOption's lable should be 'FastShipping'.");
  is(shippingOption.amount.currency, "USD", "2nd shippingOption's amount currency should be 'USD'.");
  is(shippingOption.amount.value, "30.00", "2nd shippingOption's amount value should be '30.00'.");
  ok(!shippingOption.selected, "2nd shippingOption should not be selected.");

  // checking the passed PaymentOptions parameter
  const paymentOptions = aPayment.paymentOptions;
  ok(paymentOptions.requestPayerName, "payerName option should be true");
  ok(paymentOptions.requestPayerEmail, "payerEmail option should be true");
  ok(paymentOptions.requestPayerPhone, "payerPhone option should be true");
  ok(paymentOptions.requestShipping, "requestShipping option should be true");
  is(paymentOptions.shippingType, "shipping", "shippingType option should be 'shipping'");
}

function cleanup() {
  const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);
  if (paymentSrv) {
    paymentSrv.cleanup();
  }
}
