"use strict";

async function setup() {
  await setupFormAutofillStorage();
  let prefilledGuids = await addSampleAddressesAndBasicCard();

  info("associating the card with the billing address");
  formAutofillStorage.creditCards.update(prefilledGuids.card1GUID, {
    billingAddressGUID: prefilledGuids.address1GUID,
  }, true);
}

add_task(async function test_change_shipping() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "USD", "Shipping options should be in USD");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "2", "default selected should be '2'");

    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.selectShippingOptionById, "1");

    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "1", "selected should be '1'");

    let paymentDetails = Object.assign({},
                                       PTU.Details.twoShippingOptionsEUR,
                                       PTU.Details.total1pt75EUR,
                                       PTU.Details.twoDisplayItemsEUR,
                                       PTU.Details.additionalDisplayItemsEUR);
    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: paymentDetails,
    }, PTU.ContentTasks.updateWith);
    info("added shipping change handler to change to EUR");

    await selectPaymentDialogShippingAddressByCountry(frame, "DE");
    info("changed shipping address to DE country");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingaddresschange event");

    // verify update of shippingOptions
    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "EUR",
       "Shipping options should be in EUR after the shippingaddresschange");
    is(shippingOptions.selectedOptionID, "1", "id:1 should still be selected");
    is(shippingOptions.selectedOptionValue, "1.01",
       "amount should be '1.01' after the shippingaddresschange");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});
      // verify update of total
      // Note: The update includes a modifier, and modifiers must include a total
      // so the expected total is that one
      is(content.document.querySelector("#total > currency-amount").textContent,
         "\u20AC2.50 EUR",
         "Check updated total currency amount");

      let btn = content.document.querySelector("#view-all");
      btn.click();
      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.orderDetailsShowing;
      }, "Order details show be showing now");

      let container = content.document.querySelector("order-details");
      let items = [...container.querySelectorAll(".main-list payment-details-item")]
                  .map(item => Cu.waiveXrays(item));

      // verify the updated displayItems
      is(items.length, 2, "2 display items");
      is(items[0].amountCurrency, "EUR", "First display item is in Euros");
      is(items[1].amountCurrency, "EUR", "2nd display item is in Euros");
      is(items[0].amountValue, "0.85", "First display item has 0.85 value");
      is(items[1].amountValue, "1.70", "2nd display item has 1.70 value");

      // verify the updated modifiers
      items = [...container.querySelectorAll(".footer-items-list payment-details-item")]
              .map(item => Cu.waiveXrays(item));
      is(items.length, 1, "1 additional display item");
      is(items[0].amountCurrency, "EUR", "First display item is in Euros");
      is(items[0].amountValue, "1.00", "First display item has 1.00 value");
    });

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");

    let {shippingAddress} = result.response;
    let expectedAddress = PTU.Addresses.TimBL2;
    checkPaymentAddressMatchesStorageAddress(shippingAddress, expectedAddress, "Shipping address");

    let {methodDetails} = result;
    checkPaymentMethodDetailsMatchesCard(methodDetails, PTU.BasicCards.JohnDoe, "Payment method");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_default_shippingOptions_noneSelected() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let shippingOptionDetails = Object.assign(
      deepClone(PTU.Details.twoShippingOptions), PTU.Details.total2USD
    );
    info("make sure no shipping options are selected");
    shippingOptionDetails.shippingOptions.forEach(opt => delete opt.selected);

    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: shippingOptionDetails,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "USD", "Shipping options should be in USD");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "1", "default selected should be the first");

    let shippingOptionDetailsEUR = deepClone(PTU.Details.twoShippingOptionsEUR);
    info("prepare EUR options by deselecting all and giving unique IDs");
    shippingOptionDetailsEUR.shippingOptions.forEach(opt => {
      opt.selected = false;
      opt.id += "-EUR";
    });

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: Object.assign(shippingOptionDetailsEUR, PTU.Details.total1pt75EUR),
    }, PTU.ContentTasks.updateWith);
    info("added shipping change handler to change to EUR");

    await selectPaymentDialogShippingAddressByCountry(frame, "DE");
    info("changed shipping address to DE country");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingaddresschange event");

    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "EUR", "Shipping options should be in EUR");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "1-EUR",
       "default selected should be the first");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_default_shippingOptions_allSelected() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let shippingOptionDetails = Object.assign(
      deepClone(PTU.Details.twoShippingOptions), PTU.Details.total2USD
    );
    info("make sure no shipping options are selected");
    shippingOptionDetails.shippingOptions.forEach(opt => opt.selected = true);

    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: shippingOptionDetails,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "USD", "Shipping options should be in USD");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "2", "default selected should be the last selected=true");

    let shippingOptionDetailsEUR = deepClone(PTU.Details.twoShippingOptionsEUR);
    info("prepare EUR options by selecting all and giving unique IDs");
    shippingOptionDetailsEUR.shippingOptions.forEach(opt => {
      opt.selected = true;
      opt.id += "-EUR";
    });

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
      details: Object.assign(shippingOptionDetailsEUR, PTU.Details.total1pt75EUR),
    }, PTU.ContentTasks.updateWith);
    info("added shipping change handler to change to EUR");

    await selectPaymentDialogShippingAddressByCountry(frame, "DE");
    info("changed shipping address to DE country");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);
    info("got shippingaddresschange event");

    shippingOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingOptions);
    is(shippingOptions.selectedOptionCurrency, "EUR", "Shipping options should be in EUR");
    is(shippingOptions.optionCount, 2, "there should be two shipping options");
    is(shippingOptions.selectedOptionID, "2-EUR",
       "default selected should be the last selected=true");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_no_shippingchange_without_shipping() {
  await setup();
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

    ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.ensureNoPaymentRequestEvent);
    info("added shipping change handler");

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");

    let actualShippingAddress = result.response.shippingAddress;
    ok(actualShippingAddress === null,
       "Check that shipping address is null with requestShipping:false");

    let {methodDetails} = result;
    checkPaymentMethodDetailsMatchesCard(methodDetails, PTU.BasicCards.JohnDoe, "Payment method");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_address_edit() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        options: PTU.Options.requestShippingOption,
      }
    );

    let addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("initial addressOptions: " + JSON.stringify(addressOptions));
    let selectedIndex = addressOptions.selectedOptionIndex;

    is(selectedIndex, -1, "No address should be selected initially");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    info("selecting the US address");
    await selectPaymentDialogShippingAddressByCountry(frame, "US");

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("initial addressOptions: " + JSON.stringify(addressOptions));
    selectedIndex = addressOptions.selectedOptionIndex;
    let selectedAddressGuid = addressOptions.options[selectedIndex].guid;
    let selectedAddress = formAutofillStorage.addresses.get(selectedAddressGuid);

    is(selectedIndex, 0, "First address should be selected");
    ok(selectedAddress, "Selected address does exist in the address collection");
    is(selectedAddress.country, "US", "Expected initial country value");

    info("Updating the address directly in the store");
    await formAutofillStorage.addresses.update(selectedAddress.guid, {
      country: "CA",
    }, true);

    addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("updated addressOptions: " + JSON.stringify(addressOptions));
    selectedIndex = addressOptions.selectedOptionIndex;
    let newSelectedAddressGuid = addressOptions.options[selectedIndex].guid;

    is(newSelectedAddressGuid, selectedAddressGuid, "Selected guid hasnt changed");
    selectedAddress = formAutofillStorage.addresses.get(selectedAddressGuid);

    is(selectedIndex, 0, "First address should be selected");
    is(selectedAddress.country, "CA", "Expected changed country value");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });

  await cleanupFormAutofillStorage();
});

add_task(async function test_address_removal() {
  await setup();
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        options: PTU.Options.requestShippingOption,
      }
    );

    info("selecting the US address");
    await selectPaymentDialogShippingAddressByCountry(frame, "US");

    let addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("initial addressOptions: " + JSON.stringify(addressOptions));
    let selectedIndex = addressOptions.selectedOptionIndex;
    let selectedAddressGuid = addressOptions.options[selectedIndex].guid;

    is(selectedIndex, 0, "First address should be selected");
    is(addressOptions.options.length, 2, "Should be 2 address options initially");

    info("Remove the selected address from the store");
    await formAutofillStorage.addresses.remove(selectedAddressGuid);

    await ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.promisePaymentRequestEvent);

    addressOptions =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    info("updated addressOptions: " + JSON.stringify(addressOptions));
    selectedIndex = addressOptions.selectedOptionIndex;

    is(selectedIndex, -1, "No replacement address should be selected after deletion");
    is(addressOptions.options.length, 1, "Should now be 1 address option");

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });

  await cleanupFormAutofillStorage();
});
