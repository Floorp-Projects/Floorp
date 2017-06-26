/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const paymentSrv = Cc["@mozilla.org/dom/payments/payment-request-service;1"].getService(Ci.nsIPaymentRequestService);

function emitTestFail(message) {
  sendAsyncMessage("test-fail", message);
}

function checkSimplestRequest(payRequest) {
  if (payRequest.paymentMethods.length != 1) {
    emitTestFail("paymentMethods' length should be 1.");
  }

  const methodData = payRequest.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  if (!methodData) {
    emitTestFail("Fail to get payment methodData.");
  }
  if (methodData.supportedMethods.length != 1) {
    emitTestFail("supportedMethods' length should be 1.");
  }
  const supportedMethod = methodData.supportedMethods.queryElementAt(0, Ci.nsISupportsString);
  if (supportedMethod != "basic-card") {
    emitTestFail("supported method should be 'basic-card'.");
  }

  // checking the passed PaymentDetails parameter
  const details = payRequest.paymentDetails;
  if (details.totalItem.label != "Total") {
    emitTestFail("total item's label should be 'Total'.");
  }
  if (details.totalItem.amount.currency != "USD") {
    emitTestFail("total item's currency should be 'USD'.");
  }
  if (details.totalItem.amount.value != "1.00") {
    emitTestFail("total item's value should be '1.00'.");
  }

  if (details.displayItems) {
    emitTestFail("details.displayItems should be undefined.");
  }
  if (details.modifiers) {
    emitTestFail("details.displayItems should be undefined.");
  }
  if (details.shippingOptions) {
    emitTestFail("details.shippingOptions should be undefined.");
  }

  // checking the default generated PaymentOptions parameter
  const paymentOptions = payRequest.paymentOptions;
  if (paymentOptions.requestPayerName) {
    emitTestFail("requestPayerName option should be false.");
  }
  if (paymentOptions.requestPayerEmail) {
    emitTestFail("requestPayerEmail option should be false.");
  }
  if (paymentOptions.requestPayerPhone) {
    emitTestFail("requestPayerPhone option should be false.");
  }
  if (paymentOptions.requestShipping) {
    emitTestFail("requestShipping option should be false.");
  }
  if (paymentOptions.shippingType != "shipping") {
    emitTestFail("shippingType option should be 'shipping'.")
  }
}

function checkComplexRequest(payRequest) {
  if (payRequest.paymentMethods.length != 1) {
    emitTestFail("paymentMethods' length should be 1.");
  }

  const methodData = payRequest.paymentMethods.queryElementAt(0, Ci.nsIPaymentMethodData);
  if (!methodData) {
    emitTestFail("Fail to get payment methodData.");
  }
  if (methodData.supportedMethods.length != 1) {
    emitTestFail("supportedMethods' length should be 1.");
  }
  let supportedMethod = methodData.supportedMethods.queryElementAt(0, Ci.nsISupportsString);
  if (supportedMethod != "basic-card") {
    emitTestFail("supported method should be 'basic-card'.");
  }
  const data = methodData.data;
  if (data != "{\"supportedNetworks\":[\"unionpay\",\"visa\",\"mastercard\",\"amex\",\"discover\",\"diners\",\"jcb\",\"mir\"],\"supportedTypes\":[\"prepaid\",\"debit\",\"credit\"]}") {
    emitTestFail("method data should be '{\"supportedNetworks\":[\"unionpay\",\"visa\",\"mastercard\",\"amex\",\"discover\",\"diners\",\"jcb\",\"mir\"],\"supportedTypes\":[\"prepaid\",\"debit\",\"credit\"]}', but got '" + data + "'.");
  }
  // checking the passed PaymentDetails parameter
  const details = payRequest.paymentDetails;
  if (details.id != "payment details" ) {
    emitTestFail("details.id should be 'payment details'.");
  }
  if (details.totalItem.label != "Total") {
    emitTestFail("total item's label should be 'Total'.");
  }
  if (details.totalItem.amount.currency != "USD") {
    emitTestFail("total item's currency should be 'USD'.");
  }
  if (details.totalItem.amount.value != "100.00") {
    emitTestFail("total item's value should be '100.00'.");
  }

  const displayItems = details.displayItems;
  if (!details.displayItems) {
    emitTestFail("details.displayItems should not be undefined.");
  }
  if (displayItems.length != 2) {
    emitTestFail("displayItems' length should be 2.")
  }
  let item = displayItems.queryElementAt(0, Ci.nsIPaymentItem);
  if (item.label != "First item") {
    emitTestFail("1st display item's label should be 'First item'.");
  }
  if (item.amount.currency != "USD") {
    emitTestFail("1st display item's currency should be 'USD'.");
  }
  if (item.amount.value != "60.00") {
    emitTestFail("1st display item's value should be '60.00'.");
  }
  item = displayItems.queryElementAt(1, Ci.nsIPaymentItem);
  if (item.label != "Second item") {
    emitTestFail("2nd display item's label should be 'Second item'.");
  }
  if (item.amount.currency != "USD") {
    emitTestFail("2nd display item's currency should be 'USD'.");
  }
  if (item.amount.value != "40.00") {
    emitTestFail("2nd display item's value should be '40.00'.");
  }

  const modifiers = details.modifiers;
  if (!modifiers) {
    emitTestFail("details.displayItems should not be undefined.");
  }
  if (modifiers.length != 1) {
    emitTestFail("modifiers' length should be 1.");
  }
  const modifier = modifiers.queryElementAt(0, Ci.nsIPaymentDetailsModifier);
  const modifierSupportedMethods = modifier.supportedMethods;
  if (modifierSupportedMethods.length != 1) {
    emitTestFail("modifier's supported methods length should be 1.");
  }
  supportedMethod = modifierSupportedMethods.queryElementAt(0, Ci.nsISupportsString);
  if (supportedMethod != "basic-card") {
    emitTestFail("modifier's supported method name should be 'basic-card'.");
  }
  if (modifier.total.label != "Discounted Total") {
    emitTestFail("modifier's total label should be 'Discounted Total'.");
  }
  if (modifier.total.amount.currency != "USD") {
    emitTestFail("modifier's total currency should be 'USD'.");
  }
  if (modifier.total.amount.value != "90.00") {
    emitTestFail("modifier's total value should be '90.00'.");
  }

  const additionalItems = modifier.additionalDisplayItems;
  if (additionalItems.length != 1) {
    emitTestFail("additionalDisplayItems' length should be 1.");
  }
  const additionalItem = additionalItems.queryElementAt(0, Ci.nsIPaymentItem);
  if (additionalItem.label != "basic-card discount") {
    emitTestFail("additional item's label should be 'basic-card discount'.");
  }
  if (additionalItem.amount.currency != "USD") {
    emitTestFail("additional item's currency should be 'USD'.");
  }
  if (additionalItem.amount.value != "-10.00") {
    emitTestFail("additional item's value should be '-10.00'.");
  }
  if (modifier.data != "{\"discountProgramParticipantId\":\"86328764873265\"}") {
    emitTestFail("modifier's data should be '{\"discountProgramParticipantId\":\"86328764873265\"}'.");
  }

  const shippingOptions = details.shippingOptions;
  if (!shippingOptions) {
    emitTestFail("details.shippingOptions should not be undefined.");
  }
  if (shippingOptions.length != 2) {
    emitTestFail("shippingOptions' length should be 2.");
  }
  let shippingOption = shippingOptions.queryElementAt(0, Ci.nsIPaymentShippingOption);
  if (shippingOption.id != "NormalShipping") {
    emitTestFail("1st shippingOption's id should be 'NormalShipping'.");
  }
  if (shippingOption.label != "NormalShipping") {
    emitTestFail("1st shippingOption's lable should be 'NormalShipping'.");
  }
  if (shippingOption.amount.currency != "USD") {
    emitTestFail("1st shippingOption's amount currency should be 'USD'.");
  }
  if (shippingOption.amount.value != "10.00") {
    emitTestFail("1st shippingOption's amount value should be '10.00'.");
  }
  if (!shippingOption.selected) {
    emitTestFail("1st shippingOption should be selected.");
  }
  shippingOption = shippingOptions.queryElementAt(1, Ci.nsIPaymentShippingOption);
  if (shippingOption.id != "FastShipping") {
    emitTestFail("2nd shippingOption's id should be 'FastShipping'.");
  }
  if (shippingOption.label != "FastShipping") {
    emitTestFail("2nd shippingOption's lable should be 'FastShipping'.");
  }
  if (shippingOption.amount.currency != "USD") {
    emitTestFail("2nd shippingOption's amount currency should be 'USD'.");
  }
  if (shippingOption.amount.value != "30.00") {
    emitTestFail("2nd shippingOption's amount value should be '30.00'.");
  }
  if (shippingOption.selected) {
    emitTestFail("2nd shippingOption should not be selected.");
  }

  // checking the default generated PaymentOptions parameter
  const paymentOptions = payRequest.paymentOptions;
  if (!paymentOptions.requestPayerName) {
    emitTestFail("requestPayerName option should be true.");
  }
  if (!paymentOptions.requestPayerEmail) {
    emitTestFail("requestPayerEmail option should be true.");
  }
  if (!paymentOptions.requestPayerPhone) {
    emitTestFail("requestPayerPhone option should be true.");
  }
  if (!paymentOptions.requestShipping) {
    emitTestFail("requestShipping option should be true.");
  }
  if (paymentOptions.shippingType != "shipping") {
    emitTestFail("shippingType option should be 'shipping'.")
  }
}

function checkSimplestRequestHandler() {
  const paymentEnum = paymentSrv.enumerate();
  if (!paymentEnum.hasMoreElements()) {
    emitTestFail("PaymentRequestService should have at least one payment request.");
  }
  while (paymentEnum.hasMoreElements()) {
    let payRequest = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
    if (!payRequest) {
      emitTestFail("Fail to get existing payment request.");
      break;
    }
    checkSimplestRequest(payRequest);
  }
  paymentSrv.cleanup();
  sendAsyncMessage("check-complete");
}

function checkComplexRequestHandler() {
  const paymentEnum = paymentSrv.enumerate();
  if (!paymentEnum.hasMoreElements()) {
    emitTestFail("PaymentRequestService should have at least one payment request.");
  }
  while (paymentEnum.hasMoreElements()) {
    let payRequest = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
    if (!payRequest) {
      emitTestFail("Fail to get existing payment request.");
      break;
    }
    checkComplexRequest(payRequest);
  }
  paymentSrv.cleanup();
  sendAsyncMessage("check-complete");
}

function checkMultipleRequestsHandler () {
  const paymentEnum = paymentSrv.enumerate();
  if (!paymentEnum.hasMoreElements()) {
    emitTestFail("PaymentRequestService should have at least one payment request.");
  }
  while (paymentEnum.hasMoreElements()) {
    let payRequest = paymentEnum.getNext().QueryInterface(Ci.nsIPaymentRequest);
    if (!payRequest) {
      emitTestFail("Fail to get existing payment request.");
      break;
    }
    if (payRequest.paymentDetails.id != "payment details") {
      checkSimplestRequest(payRequest);
    } else {
      checkComplexRequest(payRequest);
    }
  }
  paymentSrv.cleanup();
  sendAsyncMessage("check-complete");
}

addMessageListener("check-simplest-request", checkSimplestRequestHandler);
addMessageListener("check-complex-request", checkComplexRequestHandler);
addMessageListener("check-multiple-requests", checkMultipleRequestsHandler);

addMessageListener("teardown", function() {
  paymentSrv.cleanup();
  sendAsyncMessage("teardown-complete");
});
