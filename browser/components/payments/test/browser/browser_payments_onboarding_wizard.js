"use strict";

async function addAddress() {
  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  formAutofillStorage.addresses.add(PTU.Addresses.TimBL);
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

      info("Checking if the address page has been rendered");
      let addressSaveButton = content.document.querySelector("address-form .save-button");
      ok(content.isVisible(addressSaveButton), "Address save button is rendered");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" &&
               state["address-page"].selectedStateKey == "selectedShippingAddress";
      }, "Address page is shown first during on-boarding if there are no saved addresses");

      info("Check if the total header is visible on the address page during on-boarding");
      let header = content.document.querySelector("header");
      ok(content.isVisible(header),
         "Total Header is visible on the address page during on-boarding");
      ok(header.textContent, "Total Header contains text");

      info("Check if the page title is visible on the address page");
      let addressPageTitle = content.document.querySelector("address-form h1");
      ok(content.isVisible(addressPageTitle), "Address page title is visible");
      ok(addressPageTitle.textContent, "Address page title contains text");

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

      let basicCardTitle = content.document.querySelector("basic-card-form h1");
      ok(content.isVisible(basicCardTitle), "Basic card page title is visible");
      ok(basicCardTitle.textContent, "Basic card page title contains text");

      let cardSaveButton = content.document.querySelector("basic-card-form .save-button");
      ok(content.isVisible(cardSaveButton), "Basic card page is rendered");

      for (let [key, val] of Object.entries(PTU.BasicCards.JohnDoe)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("basic-card-form .save-button").click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "payment-summary is now visible");

      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton),
         "Payment summary page is shown after the basic card page during on boarding");
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

      info("Checking if the payment summary page is now visible");
      let cancelButton = content.document.querySelector("#cancel");
      ok(content.isVisible(cancelButton), "Payment summary page is rendered");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Payment summary page is shown first when there are saved addresses and saved cards");

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

      info("Checking if the basic card has been rendered");
      let cardSaveButton = content.document.querySelector("basic-card-form .save-button");
      ok(content.isVisible(cardSaveButton), "Basic card page is rendered");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown first if there are saved addresses during on boarding");

      info("Check if the total header is visible on the basic card page during on-boarding");
      let header = content.document.querySelector("header");
      ok(content.isVisible(header),
         "Total Header is visible on the basic card page during on-boarding");
      ok(header.textContent, "Total Header contains text");

      let cardCancelButton = content.document.querySelector("basic-card-form .cancel-button");
      ok(content.isVisible(cardCancelButton),
         "Cancel button is visible on the basic card page");
    });

    // Do not await for this task since the dialog may close before the task resolves.
    spawnPaymentDialogTask(frame, () => {
      content.document.querySelector("basic-card-form .cancel-button").click();
    });

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});
