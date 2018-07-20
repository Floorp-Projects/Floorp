"use strict";

add_task(async function setup() {
  await addSampleAddressesAndBasicCard();
});

add_task(async function test_request_shipping_present() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    for (let [shippingKey, shippingString] of [
      [null, "Shipping Address"],
      ["shipping", "Shipping Address"],
      ["delivery", "Delivery Address"],
      ["pickup", "Pickup Address"],
    ]) {
      let options = {
        requestShipping: true,
      };
      if (shippingKey) {
        options.shippingType = shippingKey;
      }
      let {win, frame} =
        await setupPaymentDialog(browser, {
          methodData: [PTU.MethodData.basicCard],
          details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
          options,
          merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        }
      );

      await spawnPaymentDialogTask(frame, async ([aShippingKey, aShippingString]) => {
        let shippingOptionPicker = content.document.querySelector("shipping-option-picker");
        ok(content.isVisible(shippingOptionPicker),
           "shipping-option-picker should be visible");
        const addressSelector = "address-picker[selected-state-key='selectedShippingAddress']";
        let shippingAddressPicker = content.document.querySelector(addressSelector);
        ok(content.isVisible(shippingAddressPicker),
           "shipping address picker should be visible");
        let shippingOption = shippingAddressPicker.querySelector("label");
        is(shippingOption.textContent, aShippingString,
           "Label should be match shipping type: " + aShippingKey);
      }, [shippingKey, shippingString]);

      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
    }
  });
});

add_task(async function test_request_shipping_not_present() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    await spawnPaymentDialogTask(frame, async () => {
      let shippingOptionPicker = content.document.querySelector("shipping-option-picker");
      ok(content.isHidden(shippingOptionPicker), "shipping-option-picker should not be visible");
      const addressSelector = "address-picker[selected-state-key='selectedShippingAddress']";
      let shippingAddress = content.document.querySelector(addressSelector);
      ok(content.isHidden(shippingAddress), "shipping address picker should not be visible");
    });

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});
