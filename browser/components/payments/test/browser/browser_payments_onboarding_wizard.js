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

    info("Closing the payment dialog");
    spawnPaymentDialogTask(frame, async function() {
      content.document.getElementById("address-page-cancel-button").click();
    });
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");

    cleanupFormAutofillStorage();
  });
});
