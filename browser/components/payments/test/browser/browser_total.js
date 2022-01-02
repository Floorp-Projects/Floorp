"use strict";

add_task(async function test_total() {
  const testTask = ({ methodData, details }) => {
    is(
      content.document.querySelector("#total > currency-amount").textContent,
      "$60.00 USD",
      "Check total currency amount"
    );
  };
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(
    PTU.ContentTasks.createAndShowRequest,
    testTask,
    args
  );
});

add_task(async function test_modifier_with_no_method_selected() {
  const testTask = async ({ methodData, details }) => {
    // There are no payment methods installed/setup so we expect the original (unmodified) total.
    is(
      content.document.querySelector("#total > currency-amount").textContent,
      "$2.00 USD",
      "Check unmodified total currency amount"
    );
  };
  const args = {
    methodData: [PTU.MethodData.bobPay, PTU.MethodData.basicCard],
    details: Object.assign(
      {},
      PTU.Details.bobPayPaymentModifier,
      PTU.Details.total2USD
    ),
  };
  await spawnInDialogForMerchantTask(
    PTU.ContentTasks.createAndShowRequest,
    testTask,
    args
  );
});

add_task(async function test_modifier_with_no_method_selected() {
  info("adding a basic-card");
  let prefilledGuids = await addSampleAddressesAndBasicCard();

  const testTask = async ({ methodData, details, prefilledGuids: guids }) => {
    is(
      content.document.querySelector("#total > currency-amount").textContent,
      "$2.00 USD",
      "Check total currency amount before selecting the credit card"
    );

    // Select the (only) payment method.
    let paymentMethodPicker = content.document.querySelector(
      "payment-method-picker"
    );
    content.fillField(
      Cu.waiveXrays(paymentMethodPicker).dropdown.popupBox,
      guids.card1GUID
    );

    await ContentTaskUtils.waitForCondition(() => {
      let currencyAmount = content.document.querySelector(
        "#total > currency-amount"
      );
      return currencyAmount.textContent == "$2.50 USD";
    }, "Wait for modified total to update");

    is(
      content.document.querySelector("#total > currency-amount").textContent,
      "$2.50 USD",
      "Check modified total currency amount"
    );
  };
  const args = {
    methodData: [PTU.MethodData.bobPay, PTU.MethodData.basicCard],
    details: Object.assign(
      {},
      PTU.Details.bobPayPaymentModifier,
      PTU.Details.total2USD
    ),
    prefilledGuids,
  };
  await spawnInDialogForMerchantTask(
    PTU.ContentTasks.createAndShowRequest,
    testTask,
    args
  );
  await cleanupFormAutofillStorage();
});
