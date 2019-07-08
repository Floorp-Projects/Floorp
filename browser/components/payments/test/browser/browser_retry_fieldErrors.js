"use strict";

/**
 * Test the merchant calling .retry() with field-specific errors.
 */

async function setup() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  // add 2 addresses and 2 cards to avoid the FTU sequence and test address errors
  let prefilledGuids = await addSampleAddressesAndBasicCard(
    [PTU.Addresses.TimBL, PTU.Addresses.TimBL2],
    [PTU.BasicCards.JaneMasterCard, PTU.BasicCards.JohnDoe]
  );

  info("associating card1 with a billing address");
  await formAutofillStorage.creditCards.update(
    prefilledGuids.card1GUID,
    {
      billingAddressGUID: prefilledGuids.address1GUID,
    },
    true
  );
  info("associating card2 with a billing address");
  await formAutofillStorage.creditCards.update(
    prefilledGuids.card2GUID,
    {
      billingAddressGUID: prefilledGuids.address1GUID,
    },
    true
  );

  return prefilledGuids;
}

add_task(async function test_retry_with_shippingAddressErrors() {
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
        details: Object.assign(
          {},
          PTU.Details.twoShippingOptions,
          PTU.Details.total60USD
        ),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await selectPaymentDialogShippingAddressByCountry(frame, "DE");

      await spawnPaymentDialogTask(
        frame,
        async ({ prefilledGuids: guids }) => {
          let paymentMethodPicker = content.document.querySelector(
            "payment-method-picker"
          );
          content.fillField(
            Cu.waiveXrays(paymentMethodPicker).dropdown.popupBox,
            guids.card2GUID
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
            state.request.paymentDetails.shippingAddressErrors.country,
            undefined,
            "Check country error string is empty"
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
            state.request.paymentDetails.shippingAddressErrors.country,
            "Can only ship to USA",
            "Check country error string in state"
          );
          ok(!state.changesPrevented, "Changes no longer prevented");
          is(
            state.page.id,
            "payment-summary",
            "Check still on payment-summary"
          );

          ok(
            content.document
              .querySelector("#payment-summary")
              .innerText.includes("Can only ship to USA"),
            "Check error visibility on summary page"
          );
          ok(
            content.document.getElementById("pay").disabled,
            "Pay button should be disabled until the field error is addressed"
          );
        }
      );

      // Add a handler to retry the payment above.
      info("Tell merchant page to retry with a country error string");
      let retryPromise = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            shippingAddress: {
              country: "Can only ship to USA",
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await retryUpdatePromise;

      info("Changing to a US address to clear the error");
      await selectPaymentDialogShippingAddressByCountry(frame, "US");

      info("Tell merchant page to retry with a regionCode error string");
      let retryPromise2 = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            shippingAddress: {
              regionCode: "Can only ship to California",
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await loginAndCompletePayment(frame);

      await spawnPaymentDialogTask(frame, async function checkRegionError() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let state = await PTU.DialogContentUtils.waitForState(
          content,
          ({ request }) => {
            return request.completeStatus === "";
          },
          "Wait for completeStatus from DOM update"
        );

        is(state.request.completeStatus, "", "Check completeStatus");
        is(
          state.request.paymentDetails.shippingAddressErrors.regionCode,
          "Can only ship to California",
          "Check regionCode error string in state"
        );
        ok(!state.changesPrevented, "Changes no longer prevented");
        is(state.page.id, "payment-summary", "Check still on payment-summary");

        ok(
          content.document
            .querySelector("#payment-summary")
            .innerText.includes("Can only ship to California"),
          "Check error visibility on summary page"
        );
        ok(
          content.document.getElementById("pay").disabled,
          "Pay button should be disabled until the field error is addressed"
        );
      });

      info(
        "Changing the shipping state to CA without changing selectedShippingAddress"
      );
      await navigateToAddShippingAddressPage(frame, {
        addLinkSelector:
          'address-picker[selected-state-key="selectedShippingAddress"] .edit-link',
      });
      await fillInShippingAddressForm(frame, { "address-level1": "CA" });
      await submitAddressForm(frame, null, { isEditing: true });

      await loginAndCompletePayment(frame);

      // We can only check the retry response after the closing as it only resolves upon complete.
      let { retryException } = await retryPromise;
      ok(
        !retryException,
        "Expect no exception to be thrown when calling retry()"
      );

      let { retryException2 } = await retryPromise2;
      ok(
        !retryException2,
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
      checkPaymentAddressMatchesStorageAddress(
        result.response.shippingAddress,
        { ...PTU.Addresses.TimBL, ...{ "address-level1": "CA" } },
        "Check response shipping address"
      );

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_retry_with_payerErrors() {
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
        details: PTU.Details.total60USD,
        options: PTU.Options.requestPayerNameEmailAndPhone,
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
            guids.card2GUID
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
            state.request.paymentDetails.payerErrors.email,
            undefined,
            "Check email error isn't present"
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
            state.request.paymentDetails.payerErrors.email,
            "You must use your employee email address",
            "Check email error string in state"
          );
          ok(!state.changesPrevented, "Changes no longer prevented");
          is(
            state.page.id,
            "payment-summary",
            "Check still on payment-summary"
          );

          ok(
            content.document
              .querySelector("#payment-summary")
              .innerText.includes("You must use your employee email address"),
            "Check error visibility on summary page"
          );
          ok(
            content.document.getElementById("pay").disabled,
            "Pay button should be disabled until the field error is addressed"
          );
        }
      );

      // Add a handler to retry the payment above.
      info("Tell merchant page to retry with a country error string");
      let retryPromise = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            payer: {
              email: "You must use your employee email address",
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await retryUpdatePromise;

      info("Changing to a different email address to clear the error");
      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.selectPayerAddressByGuid,
        prefilledGuids.address1GUID
      );

      info("Tell merchant page to retry with a phone error string");
      let retryPromise2 = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            payer: {
              phone: "Your phone number isn't valid",
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await loginAndCompletePayment(frame);

      await spawnPaymentDialogTask(frame, async function checkRegionError() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let state = await PTU.DialogContentUtils.waitForState(
          content,
          ({ request }) => {
            return request.completeStatus === "";
          },
          "Wait for completeStatus from DOM update"
        );

        is(state.request.completeStatus, "", "Check completeStatus");
        is(
          state.request.paymentDetails.payerErrors.phone,
          "Your phone number isn't valid",
          "Check regionCode error string in state"
        );
        ok(!state.changesPrevented, "Changes no longer prevented");
        is(state.page.id, "payment-summary", "Check still on payment-summary");

        ok(
          content.document
            .querySelector("#payment-summary")
            .innerText.includes("Your phone number isn't valid"),
          "Check error visibility on summary page"
        );
        ok(
          content.document.getElementById("pay").disabled,
          "Pay button should be disabled until the field error is addressed"
        );
      });

      info(
        "Changing the payer phone to be valid without changing selectedPayerAddress"
      );
      await navigateToAddAddressPage(frame, {
        addLinkSelector:
          'address-picker[selected-state-key="selectedPayerAddress"] .edit-link',
        initialPageId: "payment-summary",
        addressPageId: "payer-address-page",
      });

      let newPhoneNumber = "+16175555555";
      await fillInPayerAddressForm(frame, { tel: newPhoneNumber });

      await ContentTask.spawn(
        browser,
        {
          eventName: "payerdetailchange",
        },
        PTU.ContentTasks.promisePaymentResponseEvent
      );

      await submitAddressForm(frame, null, { isEditing: true });

      await ContentTask.spawn(
        browser,
        {
          eventName: "payerdetailchange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await loginAndCompletePayment(frame);

      // We can only check the retry response after the closing as it only resolves upon complete.
      let { retryException } = await retryPromise;
      ok(
        !retryException,
        "Expect no exception to be thrown when calling retry()"
      );

      let { retryException2 } = await retryPromise2;
      ok(
        !retryException2,
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
      let {
        "given-name": givenName,
        "additional-name": additionalName,
        "family-name": familyName,
        email,
      } = PTU.Addresses.TimBL;
      is(
        result.response.payerName,
        `${givenName} ${additionalName} ${familyName}`,
        "Check payer name"
      );
      is(result.response.payerEmail, email, "Check payer email");
      is(result.response.payerPhone, newPhoneNumber, "Check payer phone");

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_retry_with_paymentMethodErrors() {
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
        details: PTU.Details.total60USD,
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
            state.request.paymentDetails.paymentMethodErrors,
            null,
            "Check no paymentMethod errors are present"
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
            state.request.paymentDetails.paymentMethodErrors.cardSecurityCode,
            "Your CVV is incorrect",
            "Check cardSecurityCode error string in state"
          );

          ok(!state.changesPrevented, "Changes no longer prevented");
          is(
            state.page.id,
            "payment-summary",
            "Check still on payment-summary"
          );

          todo(
            content.document
              .querySelector("#payment-summary")
              .innerText.includes("Your CVV is incorrect"),
            "Bug 1491815: Check error visibility on summary page"
          );
          todo(
            content.document.getElementById("pay").disabled,
            "Bug 1491815: Pay button should be disabled until the field error is addressed"
          );
        }
      );

      // Add a handler to retry the payment above.
      info("Tell merchant page to retry with a cardSecurityCode error string");
      let retryPromise = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            paymentMethod: {
              cardSecurityCode: "Your CVV is incorrect",
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await retryUpdatePromise;

      info("Changing to a different card to clear the error");
      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.selectPaymentOptionByGuid,
        prefilledGuids.card1GUID
      );

      info(
        "Tell merchant page to retry with a billing postalCode error string"
      );
      let retryPromise2 = ContentTask.spawn(
        browser,
        {
          delayMs: 1000,
          validationErrors: {
            paymentMethod: {
              billingAddress: {
                postalCode: "Your postal code isn't valid",
              },
            },
          },
        },
        PTU.ContentTasks.addRetryHandler
      );

      await loginAndCompletePayment(frame);

      await spawnPaymentDialogTask(
        frame,
        async function checkPostalCodeError() {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            ({ request }) => {
              return request.completeStatus === "";
            },
            "Wait for completeStatus from DOM update"
          );

          is(state.request.completeStatus, "", "Check completeStatus");
          is(
            state.request.paymentDetails.paymentMethodErrors.billingAddress
              .postalCode,
            "Your postal code isn't valid",
            "Check postalCode error string in state"
          );
          ok(!state.changesPrevented, "Changes no longer prevented");
          is(
            state.page.id,
            "payment-summary",
            "Check still on payment-summary"
          );

          todo(
            content.document
              .querySelector("#payment-summary")
              .innerText.includes("Your postal code isn't valid"),
            "Bug 1491815: Check error visibility on summary page"
          );
          todo(
            content.document.getElementById("pay").disabled,
            "Bug 1491815: Pay button should be disabled until the field error is addressed"
          );
        }
      );

      info(
        "Changing the billingAddress postalCode to be valid without changing selectedPaymentCard"
      );

      await navigateToAddCardPage(frame, {
        addLinkSelector: "payment-method-picker .edit-link",
      });

      await navigateToAddAddressPage(frame, {
        addLinkSelector: ".billingAddressRow .edit-link",
        initialPageId: "basic-card-page",
        addressPageId: "billing-address-page",
      });

      let newPostalCode = "90210";
      await fillInBillingAddressForm(frame, { "postal-code": newPostalCode });

      await ContentTask.spawn(
        browser,
        {
          eventName: "paymentmethodchange",
        },
        PTU.ContentTasks.promisePaymentResponseEvent
      );

      await submitAddressForm(frame, null, {
        isEditing: true,
        nextPageId: "basic-card-page",
      });

      await spawnPaymentDialogTask(frame, async function checkErrorsCleared() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return state.request.paymentDetails.paymentMethodErrors == null;
          },
          "Check no paymentMethod errors are present"
        );
      });

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.clickPrimaryButton
      );

      await spawnPaymentDialogTask(frame, async function checkErrorsCleared() {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return state.request.paymentDetails.paymentMethodErrors == null;
          },
          "Check no card errors are present after save"
        );
      });

      // TODO: Add an `await` here after bug 1477113.
      ContentTask.spawn(
        browser,
        {
          eventName: "paymentmethodchange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await loginAndCompletePayment(frame);

      // We can only check the retry response after the closing as it only resolves upon complete.
      let { retryException } = await retryPromise;
      ok(
        !retryException,
        "Expect no exception to be thrown when calling retry()"
      );

      let { retryException2 } = await retryPromise2;
      ok(
        !retryException2,
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
        PTU.BasicCards.JaneMasterCard
      );

      let expectedBillingAddress = Object.assign({}, PTU.Addresses.TimBL, {
        "postal-code": newPostalCode,
      });

      checkPaymentMethodDetailsMatchesCard(
        result.response.details,
        expectedDetails,
        "Check response payment details"
      );
      checkPaymentAddressMatchesStorageAddress(
        result.response.details.billingAddress,
        expectedBillingAddress,
        "Check response billing address"
      );

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});
