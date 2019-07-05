"use strict";

/**
 * Test the merchant calling .retry().
 */

async function setup() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  let billingAddressGUID = await addAddressRecord(PTU.Addresses.TimBL);
  let card = Object.assign({}, PTU.BasicCards.JohnDoe, { billingAddressGUID });
  let card1GUID = await addCardRecord(card);
  return { address1GUID: billingAddressGUID, card1GUID };
}

add_task(async function test_retry_with_genericError() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(false, "Cannot test OS key store login on official builds.");
    return;
  }
  let prefilledGuids = await setup();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total60USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await spawnPaymentDialogTask(
        frame,
        async ({ prefilledGuids: guids }) => {
          let paymentMethodPicker = content.document.querySelector(
            "payment-method-picker"
          );
          content.fillField(
            Cu.waiveXrays(paymentMethodPicker).dropdown.popupBox,
            guids.card1GUID
          );
        },
        { prefilledGuids }
      );

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.setSecurityCode,
        {
          securityCode: "123",
        }
      );

      info("clicking the button to try pay the 1st time");
      await loginAndCompletePayment(frame);

      let retryUpdatePromise = spawnPaymentDialogTask(
        frame,
        async function checkDialog() {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            ({ request }) => {
              return request.completeStatus === "processing";
            },
            "Wait for completeStatus from pay button click"
          );

          is(
            state.request.completeStatus,
            "processing",
            "Check completeStatus is processing"
          );
          is(
            state.request.paymentDetails.error,
            "",
            "Check error string is empty"
          );
          ok(state.changesPrevented, "Changes prevented");

          state = await PTU.DialogContentUtils.waitForState(
            content,
            ({ request }) => {
              return request.completeStatus === "";
            },
            "Wait for completeStatus from DOM update"
          );

          is(state.request.completeStatus, "", "Check completeStatus");
          is(
            state.request.paymentDetails.error,
            "My generic error",
            "Check error string in state"
          );
          ok(!state.changesPrevented, "Changes no longer prevented");
          is(
            state.page.id,
            "payment-summary",
            "Check still on payment-summary"
          );

          ok(
            content.document
              .querySelector("payment-dialog")
              .innerText.includes("My generic error"),
            "Check error visibility"
          );
        }
      );

      // Add a handler to retry the payment above.
      info("Tell merchant page to retry with an error string");
      let retryPromise = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            error: "My generic error",
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await retryUpdatePromise;
      await loginAndCompletePayment(frame);

      // We can only check the retry response after the closing as it only resolves upon complete.
      let { retryException } = await retryPromise;
      ok(
        !retryException,
        "Expect no exception to be thrown when calling retry()"
      );

      // Add a handler to complete the payment above.
      info("acknowledging the completion from the merchant page");
      let result = await ContentTask.spawn(
        browser,
        {},
        PTU.ContentTasks.addCompletionHandler
      );

      // Verify response has the expected properties
      let expectedDetails = Object.assign(
        {
          "cc-security-code": "123",
        },
        PTU.BasicCards.JohnDoe
      );

      checkPaymentMethodDetailsMatchesCard(
        result.response.details,
        expectedDetails,
        "Check response payment details"
      );

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});
