/* eslint-disable no-shadow */

"use strict";

async function setup() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  // add an address and card to avoid the FTU sequence
  let prefilledGuids = await addSampleAddressesAndBasicCard(
    [PTU.Addresses.TimBL],
    [PTU.BasicCards.JohnDoe]
  );

  info("associating the card with the billing address");
  await formAutofillStorage.creditCards.update(
    prefilledGuids.card1GUID,
    {
      billingAddressGUID: prefilledGuids.address1GUID,
    },
    true
  );

  return prefilledGuids;
}

/*
 * Test that the payerRequested* fields are marked as required
 * on the payer address form but aren't marked as required on
 * the shipping address form.
 */
add_task(async function test_add_link() {
  await setup();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign(
          {},
          PTU.Details.twoShippingOptions,
          PTU.Details.total2USD
        ),
        options: {
          ...PTU.Options.requestShipping,
          ...PTU.Options.requestPayerNameEmailAndPhone,
        },
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await navigateToAddAddressPage(frame, {
        addLinkSelector: "address-picker.payer-related .add-link",
        initialPageId: "payment-summary",
        addressPageId: "payer-address-page",
        expectPersist: true,
      });

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let addressForm = content.document.querySelector("#payer-address-page");
        let title = addressForm.querySelector("h2");
        is(title.textContent, "Add Payer Contact", "Page title should be set");

        let saveButton = addressForm.querySelector(".save-button");
        is(saveButton.textContent, "Next", "Save button has the correct label");

        info("check that payer requested fields are marked as required");
        for (let selector of [
          "#given-name",
          "#family-name",
          "#email",
          "#tel",
        ]) {
          let element = addressForm.querySelector(selector);
          ok(element.required, selector + " should be required");
        }

        let backButton = addressForm.querySelector(".back-button");
        ok(
          content.isVisible(backButton),
          "Back button is visible on the payer address page"
        );
        backButton.click();

        await PaymentTestUtils.DialogContentUtils.waitForState(
          content,
          state => {
            return state.page.id == "payment-summary";
          },
          "Switched back to payment-summary from payer address form"
        );
      });

      await navigateToAddAddressPage(frame, {
        addLinkSelector: "address-picker.shipping-related .add-link",
        addressPageId: "shipping-address-page",
        initialPageId: "payment-summary",
        expectPersist: true,
      });

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let addressForm = content.document.querySelector(
          "#shipping-address-page"
        );
        let title = addressForm.querySelector("address-form h2");
        is(
          title.textContent,
          "Add Shipping Address",
          "Page title should be set"
        );

        let saveButton = addressForm.querySelector(".save-button");
        is(saveButton.textContent, "Next", "Save button has the correct label");

        ok(
          !addressForm.querySelector("#tel").required,
          "#tel should not be required"
        );

        let backButton = addressForm.querySelector(".back-button");
        ok(
          content.isVisible(backButton),
          "Back button is visible on the payer address page"
        );
        backButton.click();

        await PaymentTestUtils.DialogContentUtils.waitForState(
          content,
          state => {
            return state.page.id == "payment-summary";
          },
          "Switched back to payment-summary from payer address form"
        );
      });

      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
  await cleanupFormAutofillStorage();
});
