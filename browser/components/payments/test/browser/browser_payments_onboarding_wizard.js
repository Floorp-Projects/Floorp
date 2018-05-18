"use strict";

add_task(async function test_onboarding_wizard_without_saved_addresses() {
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
    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await ContentTaskUtils.waitForCondition(() =>
        content.document.getElementById("address-page-cancel-button"), "Address page is rendered");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page";
      }, "Address page is shown first if there are no saved addresses");
    });

    info("Check if the total header is visible on the address page during on-boarding");
    let isHeaderVisible =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
    ok(isHeaderVisible, "Total Header is visible on the address page during on-boarding");

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, async function() {
      content.document.getElementById("address-page-cancel-button").click();
    });
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_onboarding_wizard_with_saved_addresses() {
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
    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await ContentTaskUtils.waitForCondition(() => content.document.getElementById("cancel"),
                                              "Payment summary page is rendered");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Payment summary page is shown when there are saved addresses");
    });

    info("Check if the total header is visible on payments summary page");
    let isHeaderVisible =
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
    ok(isHeaderVisible, "Total Header is visible on the payment summary page");

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
          await ContentTaskUtils.waitForCondition(() =>
            content.document.querySelector("address-form .back-button"),
                                                  "Address page is rendered");
          await PTU.DialogContentUtils.waitForState(content, (state) => {
            return state.page.id == "address-page";
          }, "Address page is shown");
        } else {
          await ContentTaskUtils.waitForCondition(() =>
            content.document.querySelector("basic-card-form .back-button"),
                                                  "Basic card page is rendered");
          await PTU.DialogContentUtils.waitForState(content, (state) => {
            return state.page.id == "basic-card-page";
          }, "Basic card page is shown");
        }
      }, button);

      isHeaderVisible =
        await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.isElementVisible, "header");
      ok(!isHeaderVisible, "Total Header is hidden on the address page");

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
