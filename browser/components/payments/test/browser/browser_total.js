"use strict";

add_task(async function test_total() {
  const testTask = ({methodData, details}) => {
    is(content.document.querySelector("#total > currency-amount").textContent,
       "$60.00",
       "Check total currency amount");
  };
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});

add_task(async function test_modifier_with_no_method_selected() {
  const testTask = async ({methodData, details}) => {
    // There are no payment methods installed/setup so we expect the original (unmodified) total.
    is(content.document.querySelector("#total > currency-amount").textContent,
       "$2.00",
       "Check unmodified total currency amount");
  };
  const args = {
    methodData: [PTU.MethodData.bobPay, PTU.MethodData.basicCard],
    details: PTU.Details.bobPayPaymentModifier,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
});

add_task(async function test_modifier_with_no_method_selected() {
  info("adding a basic-card");
  await addSampleAddressesAndBasicCard();

  const testTask = async ({methodData, details}) => {
    // We expect the *only* payment method (the one basic-card) to be selected initially.
    is(content.document.querySelector("#total > currency-amount").textContent,
       "$2.50",
       "Check modified total currency amount");
  };
  const args = {
    methodData: [PTU.MethodData.bobPay, PTU.MethodData.basicCard],
    details: PTU.Details.bobPayPaymentModifier,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, testTask, args);
  await cleanupFormAutofillStorage();
});
