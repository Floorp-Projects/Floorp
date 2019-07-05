/**
 * Test saving/updating address records with fields sometimes not visible to the user.
 */

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

add_task(async function test_hiddenFieldNotSaved() {
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
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      let newAddress = Object.assign({}, PTU.Addresses.TimBL);
      newAddress["given-name"] = "hiddenFields";

      let shippingAddressChangePromise = ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      let options = {
        addLinkSelector: "address-picker.shipping-related .add-link",
        initialPageId: "payment-summary",
        addressPageId: "shipping-address-page",
        expectPersist: true,
        isEditing: false,
      };
      await navigateToAddAddressPage(frame, options);
      info("navigated to add address page");
      await fillInShippingAddressForm(frame, newAddress, options);
      info("filled in TimBL address");

      await spawnPaymentDialogTask(frame, async args => {
        let addressForm = content.document.getElementById(
          "shipping-address-page"
        );
        let countryField = addressForm.querySelector("#country");
        await content.fillField(countryField, "DE");
      });
      info("changed selected country to Germany");

      await submitAddressForm(frame, null, options);
      await shippingAddressChangePromise;
      info("got shippingaddresschange event");

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let { savedAddresses } = await PTU.DialogContentUtils.getCurrentState(
          content
        );
        is(Object.keys(savedAddresses).length, 2, "2 saved addresses");
        let savedAddress = Object.values(savedAddresses).find(
          address => address["given-name"] == "hiddenFields"
        );
        ok(savedAddress, "found the saved address");
        is(savedAddress.country, "DE", "check country");
        is(
          savedAddress["address-level2"],
          PTU.Addresses.TimBL["address-level2"],
          "check address-level2"
        );
        is(
          savedAddress["address-level1"],
          undefined,
          "address-level1 should not be saved"
        );
      });

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
  await cleanupFormAutofillStorage();
});

add_task(async function test_hiddenFieldRemovedWhenCountryChanged() {
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
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      let shippingAddressChangePromise = ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await spawnPaymentDialogTask(frame, async args => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let picker = content.document.querySelector(
          "address-picker[selected-state-key='selectedShippingAddress']"
        );
        Cu.waiveXrays(picker).dropdown.popupBox.focus();
        EventUtils.synthesizeKey(
          PTU.Addresses.TimBL["given-name"],
          {},
          content.window
        );

        let editLink = content.document.querySelector(
          "address-picker .edit-link"
        );
        is(editLink.textContent, "Edit", "Edit link text");

        editLink.click();

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return (
              state.page.id == "shipping-address-page" &&
              !!state["shipping-address-page"].guid
            );
          },
          "Check edit page state"
        );

        let addressForm = content.document.getElementById(
          "shipping-address-page"
        );
        let countryField = addressForm.querySelector("#country");
        await content.fillField(countryField, "DE");
        info("changed selected country to Germany");
      });

      let options = {
        isEditing: true,
        addressPageId: "shipping-address-page",
      };
      await submitAddressForm(frame, null, options);
      await shippingAddressChangePromise;
      info("got shippingaddresschange event");

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let { savedAddresses } = await PTU.DialogContentUtils.getCurrentState(
          content
        );
        is(Object.keys(savedAddresses).length, 1, "1 saved address");
        let savedAddress = Object.values(savedAddresses)[0];
        is(savedAddress.country, "DE", "check country");
        is(
          savedAddress["address-level2"],
          PTU.Addresses.TimBL["address-level2"],
          "check address-level2"
        );
        is(
          savedAddress["address-level1"],
          undefined,
          "address-level1 should not be saved"
        );
      });

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
  await cleanupFormAutofillStorage();
});

add_task(async function test_hiddenNonShippingFieldsPreservedUponEdit() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  // add a Brazilian address (since it uses all fields) and card to avoid the FTU sequence
  let prefilledGuids = await addSampleAddressesAndBasicCard(
    [PTU.Addresses.TimBR],
    [PTU.BasicCards.JohnDoe]
  );
  let expected = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );

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
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await selectPaymentDialogShippingAddressByCountry(frame, "BR");

      await navigateToAddShippingAddressPage(frame, {
        addLinkSelector:
          'address-picker[selected-state-key="selectedShippingAddress"] .edit-link',
        addressPageId: "shipping-address-page",
        initialPageId: "payment-summary",
      });

      await spawnPaymentDialogTask(frame, async () => {
        let givenNameField = content.document.querySelector(
          "#shipping-address-page #given-name"
        );
        await content.fillField(givenNameField, "Timothy-edit");
      });

      let options = {
        isEditing: true,
      };
      await submitAddressForm(frame, null, options);

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );

  Object.assign(expected, PTU.Addresses.TimBR, {
    "given-name": "Timothy-edit",
    name: "Timothy-edit Jo\u{00E3}o Berners-Lee",
  });
  let actual = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );
  isnot(actual.email, "", "Check email isn't empty");
  // timeLastModified changes and isn't relevant
  delete actual.timeLastModified;
  delete expected.timeLastModified;
  SimpleTest.isDeeply(actual, expected, "Check profile didn't lose fields");

  await cleanupFormAutofillStorage();
});

add_task(async function test_hiddenNonPayerFieldsPreservedUponEdit() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  // add a Brazilian address (since it uses all fields) and card to avoid the FTU sequence
  let prefilledGuids = await addSampleAddressesAndBasicCard(
    [PTU.Addresses.TimBR],
    [PTU.BasicCards.JohnDoe]
  );
  let expected = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total2USD),
        options: {
          requestPayerEmail: true,
        },
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await navigateToAddAddressPage(frame, {
        addLinkSelector:
          'address-picker[selected-state-key="selectedPayerAddress"] .edit-link',
        initialPageId: "payment-summary",
        addressPageId: "payer-address-page",
      });

      info("Change the email address");
      await spawnPaymentDialogTask(
        frame,
        `async () => {
      let emailField = content.document.querySelector("#payer-address-page #email");
      await content.fillField(emailField, "new@example.com");
    }`
      );

      let options = {
        isEditing: true,
      };
      await submitAddressForm(frame, null, options);

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );

  Object.assign(expected, PTU.Addresses.TimBR, {
    email: "new@example.com",
  });
  let actual = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );
  // timeLastModified changes and isn't relevant
  delete actual.timeLastModified;
  delete expected.timeLastModified;
  SimpleTest.isDeeply(
    actual,
    expected,
    "Check profile didn't lose fields and change was made"
  );

  await cleanupFormAutofillStorage();
});

add_task(async function test_hiddenNonBillingAddressFieldsPreservedUponEdit() {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  // add a Brazilian address (since it uses all fields) and card to avoid the FTU sequence
  let prefilledGuids = await addSampleAddressesAndBasicCard(
    [PTU.Addresses.TimBR],
    [PTU.BasicCards.JohnDoe]
  );
  let expected = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );

  info("associating the card with the billing address");
  await formAutofillStorage.creditCards.update(
    prefilledGuids.card1GUID,
    {
      billingAddressGUID: prefilledGuids.address1GUID,
    },
    true
  );

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
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await navigateToAddCardPage(frame, {
        addLinkSelector: "payment-method-picker .edit-link",
      });

      await navigateToAddAddressPage(frame, {
        addLinkSelector: ".billingAddressRow .edit-link",
        initialPageId: "basic-card-page",
        addressPageId: "billing-address-page",
      });

      await spawnPaymentDialogTask(
        frame,
        `async () => {
      let givenNameField = content.document.querySelector(
        "#billing-address-page #given-name"
      );
      await content.fillField(givenNameField, "Timothy-edit");
    }`
      );

      let options = {
        isEditing: true,
        nextPageId: "basic-card-page",
      };
      await submitAddressForm(frame, null, options);

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );

  Object.assign(expected, PTU.Addresses.TimBR, {
    "given-name": "Timothy-edit",
    name: "Timothy-edit Jo\u{00E3}o Berners-Lee",
  });
  let actual = await formAutofillStorage.addresses.get(
    prefilledGuids.address1GUID
  );
  // timeLastModified changes and isn't relevant
  delete actual.timeLastModified;
  delete expected.timeLastModified;
  SimpleTest.isDeeply(actual, expected, "Check profile didn't lose fields");

  await cleanupFormAutofillStorage();
});
