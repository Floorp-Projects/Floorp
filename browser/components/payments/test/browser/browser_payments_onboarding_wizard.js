"use strict";

async function addAddress() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  formAutofillStorage.addresses.add(PTU.Addresses.TimBL);
  await onChanged;
}

async function addBasicCard() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  formAutofillStorage.creditCards.add(PTU.BasicCards.JohnDoe);
  await onChanged;
}

add_task(async function test_onboarding_wizard_without_saved_addresses_and_saved_cards() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    cleanupFormAutofillStorage();

    info("Opening the payment dialog");
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" &&
               state.page.selectedStateKey[0] == "selectedShippingAddress";
      }, "Address page is shown first during on-boarding if there are no saved addresses");

      info("Checking if the address page has been rendered");
      let addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton), "Address save button is rendered");

      info("Check if the total header is visible on the address page during on-boarding");
      let header = content.document.querySelector("header");
      ok(content.isVisible(header),
         "Total Header is visible on the address page during on-boarding");
      ok(header.textContent, "Total Header contains text");

      info("Check if the page title is visible on the address page");
      let addressPageTitle = content.document.querySelector("address-form h2");
      ok(content.isVisible(addressPageTitle), "Address page title is visible");
      is(addressPageTitle.textContent, "Add Shipping Address",
         "Address page title is correctly shown");

      let addressCancelButton = content.document.querySelector("address-form .cancel-button");
      ok(content.isVisible(addressCancelButton),
         "The cancel button on the address page is visible");

      for (let [key, val] of Object.entries(PTU.Addresses.TimBL2)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("address-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown after the address page during on boarding");

      let cardSaveButton = content.document.querySelector("basic-card-form .save-button");
      ok(content.isVisible(cardSaveButton), "Basic card page is rendered");

      let basicCardTitle = content.document.querySelector("basic-card-form h2");
      ok(content.isVisible(basicCardTitle), "Basic card page title is visible");
      is(basicCardTitle.textContent, "Add Credit Card", "Basic card page title is correctly shown");

      info("Check if the correct billing address is selected in the basic card page");
      PTU.DialogContentUtils.waitForState((state) => {
        let billingAddressSelect = content.document.querySelector("#billingAddressGUID");
        return state.selectedShippingAddress == billingAddressSelect.value;
      }, "Shipping address is selected as the billing address");

      for (let [key, val] of Object.entries(PTU.BasicCards.JohnDoe)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("basic-card-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Payment summary page is shown after the basic card page during on boarding");

      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton),
         "Payment summary page is rendered");
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_onboarding_wizard_with_saved_addresses_and_saved_cards() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    addSampleAddressesAndBasicCard();

    info("Opening the payment dialog");
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Payment summary page is shown first when there are saved addresses and saved cards");

      info("Checking if the payment summary page is now visible");
      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton), "Payment summary page is rendered");

      info("Check if the total header is visible on payments summary page");
      let header = content.document.querySelector("header");
      ok(content.isVisible(header),
         "Total Header is visible on the payment summary page");
      ok(header.textContent, "Total Header contains text");

      // Click on the Add/Edit buttons in the payment summary page to check if
      // the total header is visible on the address page and the basic card page.
      let buttons = [
        "address-picker[selected-state-key='selectedShippingAddress'] .add-link",
        "address-picker[selected-state-key='selectedShippingAddress'] .edit-link",
        "payment-method-picker .add-link",
        "payment-method-picker .edit-link",
      ];
      for (let button of buttons) {
        content.document.querySelector(button).click();
        if (button.startsWith("address")) {
          await PTU.DialogContentUtils.waitForState(content, (state) => {
            return state.page.id == "address-page";
          }, "Address page is shown");
        } else {
          await PTU.DialogContentUtils.waitForState(content, (state) => {
            return state.page.id == "basic-card-page";
          }, "Basic card page is shown");
        }

        ok(!content.isVisible(header),
           "Total Header is hidden on the address/basic card page");

        if (button.startsWith("address")) {
          content.document.querySelector("address-form .back-button").click();
        } else {
          content.document.querySelector("basic-card-form .back-button").click();
        }
      }
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});

add_task(async function test_onboarding_wizard_with_saved_addresses_and_no_saved_cards() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    addAddress();

    info("Opening the payment dialog");
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown first if there are saved addresses during on boarding");

      info("Checking if the basic card page has been rendered");
      let cardSaveButton = content.document.querySelector("basic-card-form .save-button");
      ok(content.isVisible(cardSaveButton), "Basic card page is rendered");

      info("Check if the total header is visible on the basic card page during on-boarding");
      let header = content.document.querySelector("header");
      ok(content.isVisible(header),
         "Total Header is visible on the basic card page during on-boarding");
      ok(header.textContent, "Total Header contains text");

      let cardCancelButton = content.document.querySelector("basic-card-form .cancel-button");
      ok(content.isVisible(cardCancelButton),
         "Cancel button is visible on the basic card page");

      let cardBackButton = content.document.querySelector("basic-card-form .back-button");
      ok(!content.isVisible(cardBackButton),
         "Back button is hidden on the basic card page when it is shown first during onboarding");
    });

    // Do not await for this task since the dialog may close before the task resolves.
    spawnPaymentDialogTask(frame, () => {
      content.document.querySelector("basic-card-form .cancel-button").click();
    });

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});

add_task(async function test_onboarding_wizard_without_saved_address_with_saved_cards() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    cleanupFormAutofillStorage();
    addBasicCard();

    info("Opening the payment dialog");
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page";
      }, "Address page is shown first if there are saved addresses during on boarding");

      info("Checking if the address page has been rendered");
      let addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton), "Address save button is rendered");

      for (let [key, val] of Object.entries(PTU.Addresses.TimBL2)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("address-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "payment-summary is now visible");

      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton), "Payment summary page is shown next");
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
    cleanupFormAutofillStorage();
  });
});

add_task(async function test_onboarding_wizard_with_requestShipping_turned_off() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    cleanupFormAutofillStorage();

    info("Opening the payment dialog");
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" &&
               state.page.selectedStateKey[0] == "basic-card-page" &&
               state.page.selectedStateKey[1] == "billingAddressGUID";
      // eslint-disable-next-line max-len
      }, "Billing address page is shown first during on-boarding if requestShipping is turned off");

      info("Checking if the billing address page has been rendered");
      let addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton),
         "Address save button is rendered");

      info("Check if the page title is visible on the address page");
      let addressPageTitle = content.document.querySelector("address-form h2");
      ok(content.isVisible(addressPageTitle), "Address page title is visible");
      is(addressPageTitle.textContent, "Add Billing Address",
         "Address page title is correctly shown");

      for (let [key, val] of Object.entries(PTU.Addresses.TimBL2)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("address-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      // eslint-disable-next-line max-len
      }, "Basic card page is shown after the billing address page during onboarding if requestShipping is turned off");

      let cardSaveButton = content.document.querySelector("basic-card-form .save-button");
      ok(content.isVisible(cardSaveButton), "Basic card page is rendered");

      info("Check if the correct billing address is selected in the basic card page");
      PTU.DialogContentUtils.waitForState((state) => {
        let billingAddressSelect = content.document.querySelector("#billingAddressGUID");
        return state["basic-card-page"].billingAddressGUID == billingAddressSelect.value;
      }, "Billing Address is correctly shown");

      for (let [key, val] of Object.entries(PTU.BasicCards.JohnDoe)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("basic-card-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "payment-summary is shown after the basic card page during on boarding");

      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton), "Payment summary page is rendered");
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});

add_task(async function test_on_boarding_wizard_with_requestShipping_turned_off_with_saved_cards() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    cleanupFormAutofillStorage();
    addBasicCard();

    info("Opening the payment dialog");
    let {win, frame} =
        await setupPaymentDialog(browser, {
          methodData: [PTU.MethodData.basicCard],
          details: PTU.Details.total60USD,
          merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        });

    await spawnPaymentDialogTask(frame, async function() {
      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton),
       // eslint-disable-next-line max-len
         "Payment summary page is shown if requestShipping is turned off and there's a saved card but no saved address");
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});

add_task(async function test_back_button_on_basic_card_page_during_onboarding() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    cleanupFormAutofillStorage();

    info("Opening the payment dialog");
    let {win, frame} =
        await setupPaymentDialog(browser, {
          methodData: [PTU.MethodData.basicCard],
          details: PTU.Details.total60USD,
          merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        });

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page";
      }, "Address page is shown first if there are saved addresses during on boarding");

      info("Checking if the address page has been rendered");
      let addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton), "Address save button is rendered");

      for (let [key, val] of Object.entries(PTU.Addresses.TimBL2)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("address-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown next");

      info("Checking if basic card page is rendered");
      let basicCardBackButton = content.document.querySelector("basic-card-form .back-button");
      ok(content.isVisible(basicCardBackButton), "Back button is visible on the basic card page");

      info("Partially fill basic card form");
      let field = content.document.getElementById("cc-number");
      field.value = PTU.BasicCards.JohnDoe["cc-number"];

      info("Clicking on the back button to edit address saved in the previous step");
      basicCardBackButton.click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" &&
               state["address-page"].guid == state["basic-card-page"].billingAddressGUID;
      }, "Address page is shown again");

      info("Checking if the address page has been rendered");
      addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton), "Address save button is rendered");

      info("Checking if the address saved in the last step is correctly loaded in the form");
      field = content.document.getElementById("given-name");
      ok(field.value, PTU.Addresses.TimBL2["given-name"],
         "Given name field value is correctly loaded");

      info("Editing the address and saving again");
      field.value = "John";
      addressSaveButton.click();

      info("Checking if the address was correctly edited");
      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page" &&
               // eslint-disable-next-line max-len
               state.savedAddresses[state["basic-card-page"].billingAddressGUID]["given-name"] == "John";
      }, "Address was correctly edited and saved");

      // eslint-disable-next-line max-len
      info("Checking if the basic card form is now rendered and if the field values from before are preserved");
      let basicCardCancelButton = content.document.querySelector("basic-card-form .cancel-button");
      ok(content.isVisible(basicCardCancelButton),
         "Cancel button is visible on the basic card page");
      field = content.document.getElementById("cc-number");
      ok(field.value, PTU.BasicCards.JohnDoe["cc-number"], "Values in the form are preserved");
    });

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});
