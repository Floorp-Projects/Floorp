"use strict";

add_task(async function test_serializeRequest_displayItems() {
  const testTask = ({methodData, details}) => {
    let contentWin = Cu.waiveXrays(content);
    let store = contentWin.document.querySelector("payment-dialog").requestStore;
    let state = store && store.getState();
    ok(state, "got request store state");

    let expected = details;
    let actual = state.request.paymentDetails;
    if (expected.displayItems) {
      is(actual.displayItems.length, expected.displayItems.length, "displayItems have same length");
      for (let i = 0; i < actual.displayItems.length; i++) {
        let item = actual.displayItems[i], expectedItem = expected.displayItems[i];
        is(item.label, expectedItem.label, "displayItem label matches");
        is(item.amount.value, expectedItem.amount.value, "displayItem label matches");
        is(item.amount.currency, expectedItem.amount.currency, "displayItem label matches");
      }
    } else {
      is(actual.displayItems, null, "falsey input displayItems is serialized to null");
    }
  };
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.twoDisplayItems,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});

add_task(async function test_serializeRequest_shippingOptions() {
  const testTask = ({methodData, details}) => {
    let contentWin = Cu.waiveXrays(content);
    let store = contentWin.document.querySelector("payment-dialog").requestStore;
    let state = store && store.getState();
    ok(state, "got request store state");

    // let expected = details;
    // let actual = state.request.paymentDetails;
    // if (expected.shippingOptions) {
    //   is(actual.shippingOptions.length, expected.shippingOptions.length,
    //      "shippingOptions have same length");
    //   for (let i = 0; i < actual.shippingOptions.length; i++) {
    //     let item = actual.shippingOptions[i], expectedItem = expected.shippingOptions[i];
    //     is(item.label, expectedItem.label, "shippingOption label matches");
    //     is(item.amount.value, expectedItem.amount.value, "shippingOption label matches");
    //     is(item.amount.currency, expectedItem.amount.currency, "shippingOption label matches");
    //   }
    // } else {
    //   is(actual.shippingOptions, null, "falsey input shippingOptions is serialized to null");
    // }
  };

  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.twoShippingOptions,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});

add_task(async function test_serializeRequest_paymentMethods() {
  const testTask = ({methodData, details}) => {
    let contentWin = Cu.waiveXrays(content);
    let store = contentWin.document.querySelector("payment-dialog").requestStore;
    let state = store && store.getState();
    ok(state, "got request store state");

    let result = state.request;

    is(result.paymentMethods.length, 2, "Correct number of payment methods");
    ok(result.paymentMethods[0].supportedMethods && result.paymentMethods[1].supportedMethods,
       "Both payment methods look valid");
  };
  const args = {
    methodData: [PTU.MethodData.basicCard, PTU.MethodData.bobPay],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});

add_task(async function test_serializeRequest_modifiers() {
  const testTask = ({methodData, details}) => {
    let contentWin = Cu.waiveXrays(content);
    let store = contentWin.document.querySelector("payment-dialog").requestStore;
    let state = store && store.getState();
    ok(state, "got request store state");

    let expected = details;
    let actual = state.request.paymentDetails;

    is(actual.modifiers.length, expected.modifiers.length,
       "modifiers have same length");
    for (let i = 0; i < actual.modifiers.length; i++) {
      let item = actual.modifiers[i], expectedItem = expected.modifiers[i];
      is(item.supportedMethods, expectedItem.supportedMethods, "modifier supportedMethods matches");

      is(item.additionalDisplayItems[0].label, expectedItem.additionalDisplayItems[0].label,
         "additionalDisplayItems label matches");
      is(item.additionalDisplayItems[0].amount.value,
         expectedItem.additionalDisplayItems[0].amount.value,
         "additionalDisplayItems amount value matches");
      is(item.additionalDisplayItems[0].amount.currency,
         expectedItem.additionalDisplayItems[0].amount.currency,
         "additionalDisplayItems amount currency matches");

      is(item.total.label, expectedItem.total.label, "modifier total label matches");
      is(item.total.amount.value, expectedItem.total.amount.value, "modifier label matches");
      is(item.total.amount.currency, expectedItem.total.amount.currency,
         "modifier total currency matches");
    }
  };

  const args = {
    methodData: [PTU.MethodData.basicCard, PTU.MethodData.bobPay],
    details: PTU.Details.bobPayPaymentModifier,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});
