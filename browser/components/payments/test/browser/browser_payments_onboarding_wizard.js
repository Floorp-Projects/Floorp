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

    info("Checking if the address page has been rendered");
    let isAddressPageSaveButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "address-form .save-button");
    ok(isAddressPageSaveButtonVisible, "Address page is rendered");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" &&
               state["address-page"].selectedStateKey == "selectedShippingAddress";
      }, "Address page is shown first during on-boarding if there are no saved addresses");
    });

    info("Check if the total header is visible on the address page during on-boarding");
    let isHeaderVisible =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
    ok(isHeaderVisible, "Total Header is visible on the address page during on-boarding");
    let headerTextContent =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getElementTextContent, "header");
    ok(headerTextContent, "Total Header contains text");

    info("Check if the page title is visible on the address page");
    let isAddressPageTitleVisible = spawnPaymentDialogTask(frame,
                                                           PTU.DialogContentTasks.isElementVisible,
                                                           "address-form h1");
    ok(isAddressPageTitleVisible, "Address page title is visible");
    let titleTextContent =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getElementTextContent,
                             "address-form h1");
    ok(titleTextContent, "Address page title contains text");

    let isAddressPageCancelButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "address-form .cancel-button");
    ok(isAddressPageCancelButtonVisible, "The cancel button on the address page is visible");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      for (let [key, val] of Object.entries(PTU.Addresses.TimBL2)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("address-form .save-button").click();
    });

    let isBasicCardPageSaveButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "basic-card-form .save-button");
    ok(isBasicCardPageSaveButtonVisible, "Basic card page is rendered");

    let isBasicCardPageTitleVisible = spawnPaymentDialogTask(frame,
                                                             // eslint-disable-next-line max-len
                                                             PTU.DialogContentTasks.isElementVisible,
                                                             "basic-card-form h1");
    ok(isBasicCardPageTitleVisible, "Basic card page title is visible");
    titleTextContent =
      spawnPaymentDialogTask(frame,
                             PTU.DialogContentTasks.getElementTextContent,
                             "basic-card-form h1");
    ok(titleTextContent, "Basic card page title contains text");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown after the address page during on boarding");

      for (let [key, val] of Object.entries(PTU.BasicCards.JohnDoe)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }
      content.document.querySelector("basic-card-form .save-button").click();
    });

    let isPaymentSummaryCancelButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "#cancel");
    ok(isPaymentSummaryCancelButtonVisible,
       "Payment summary page is shown after the basic card page during on boarding");

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, async function() {
      content.document.getElementById("cancel").click();
    });
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

    info("Checking if the payment summary page is now visible");
    let isPaymentSummaryCancelButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "#cancel");
    ok(isPaymentSummaryCancelButtonVisible, "Payment summary page is rendered");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Payment summary page is shown first when there are saved addresses and saved cards");
    });

    info("Check if the total header is visible on payments summary page");
    let isHeaderVisible =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
    ok(isHeaderVisible, "Total Header is visible on the payment summary page");
    let headerTextContent =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getElementTextContent, "header");
    ok(headerTextContent, "Total Header contains text");

    // Click on the Add/Edit buttons in the payment summary page to check if
    // the total header is visible on the address page and the basic card page.
    let buttons = ["address-picker[selected-state-key='selectedShippingAddress'] .add-link",
      "address-picker[selected-state-key='selectedShippingAddress'] .edit-link",
      "payment-method-picker .add-link",
      "payment-method-picker .edit-link"];
    for (let button of buttons) {
      // eslint-disable-next-line no-shadow
      await spawnPaymentDialogTask(frame, async function(button) {
        let {
          PaymentTestUtils: PTU,
        } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

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
      }, button);

      isHeaderVisible =
        await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
      ok(!isHeaderVisible, "Total Header is hidden on the address/basic card page");

      if (button.startsWith("address")) {
        spawnPaymentDialogTask(frame, async function() {
          content.document.querySelector("address-form .back-button").click();
        });
      } else {
        spawnPaymentDialogTask(frame, async function() {
          content.document.querySelector("basic-card-form .back-button").click();
        });
      }
    }

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, async function() {
      content.document.getElementById("cancel").click();
    });
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

    info("Checking if the basic card has been rendered");
    let isBasicCardPageSaveButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "basic-card-form .save-button");
    ok(isBasicCardPageSaveButtonVisible, "Basic card page is rendered");

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page";
      }, "Basic card page is shown first if there are saved addresses during on boarding");
    });

    info("Check if the total header is visible on the basic card page during on-boarding");
    let isHeaderVisible =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
    ok(isHeaderVisible, "Total Header is visible on the basic card page during on-boarding");
    let headerTextContent =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getElementTextContent, "header");
    ok(headerTextContent, "Total Header contains text");

    let isBasicCardPageCancelButtonVisible =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible,
                                   "basic-card-form .cancel-button");
    ok(isBasicCardPageCancelButtonVisible, "Cancel button is visible on the basic card page");

    spawnPaymentDialogTask(frame, async function() {
      content.document.querySelector("basic-card-form .cancel-button").click();
    });
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});
