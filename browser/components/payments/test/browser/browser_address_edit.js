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
 * Test that we can correctly add an address and elect for it to be saved or temporary
 */
add_task(async function test_add_link() {
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
          PTU.Details.total2USD
        ),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      info("setup got prefilledGuids: " + JSON.stringify(prefilledGuids));
      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let {
          tempAddresses,
          savedAddresses,
        } = await PTU.DialogContentUtils.getCurrentState(content);
        is(
          Object.keys(tempAddresses).length,
          0,
          "No temporary addresses at the start of test"
        );
        is(
          Object.keys(savedAddresses).length,
          1,
          "1 saved address at the start of test"
        );
      });

      let testOptions = [
        { setPersistCheckedValue: true, expectPersist: true },
        { setPersistCheckedValue: false, expectPersist: false },
      ];
      let newAddress = Object.assign({}, PTU.Addresses.TimBL2);
      // Emails aren't part of shipping addresses
      delete newAddress.email;

      for (let options of testOptions) {
        let shippingAddressChangePromise = ContentTask.spawn(
          browser,
          {
            eventName: "shippingaddresschange",
          },
          PTU.ContentTasks.awaitPaymentEventPromise
        );

        await manuallyAddShippingAddress(frame, newAddress, options);
        await shippingAddressChangePromise;
        info("got shippingaddresschange event");

        await spawnPaymentDialogTask(
          frame,
          async ({ address, options, prefilledGuids }) => {
            let { PaymentTestUtils: PTU } = ChromeUtils.import(
              "resource://testing-common/PaymentTestUtils.jsm"
            );

            let newAddresses = await PTU.DialogContentUtils.waitForState(
              content,
              state => {
                return state.tempAddresses && state.savedAddresses;
              }
            );
            let colnName = options.expectPersist
              ? "savedAddresses"
              : "tempAddresses";
            // remove any pre-filled entries
            delete newAddresses[colnName][prefilledGuids.address1GUID];

            let addressGUIDs = Object.keys(newAddresses[colnName]);
            is(addressGUIDs.length, 1, "Check there is one address");
            let resultAddress = newAddresses[colnName][addressGUIDs[0]];
            for (let [key, val] of Object.entries(address)) {
              is(resultAddress[key], val, "Check " + key);
            }
          },
          { address: newAddress, options, prefilledGuids }
        );
      }

      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
  await cleanupFormAutofillStorage();
});

add_task(async function test_edit_link() {
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

      const EXPECTED_ADDRESS = {
        "given-name": "Jaws",
        "family-name": "swaJ",
        organization: "aliizoM",
      };

      info("setup got prefilledGuids: " + JSON.stringify(prefilledGuids));
      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let {
          tempAddresses,
          savedAddresses,
        } = await PTU.DialogContentUtils.getCurrentState(content);
        is(
          Object.keys(tempAddresses).length,
          0,
          "No temporary addresses at the start of test"
        );
        is(
          Object.keys(savedAddresses).length,
          1,
          "1 saved address at the start of test"
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

        let addressForm = content.document.querySelector(
          "#shipping-address-page"
        );
        ok(content.isVisible(addressForm), "Shipping address form is visible");

        let title = addressForm.querySelector("h2");
        is(
          title.textContent,
          "Edit Shipping Address",
          "Page title should be set"
        );

        let saveButton = addressForm.querySelector(".save-button");
        is(
          saveButton.textContent,
          "Update",
          "Save button has the correct label"
        );
      });

      let editOptions = {
        checkboxSelector: "#shipping-address-page .persist-checkbox",
        isEditing: true,
        expectPersist: true,
      };
      await fillInShippingAddressForm(frame, EXPECTED_ADDRESS, editOptions);
      await verifyPersistCheckbox(frame, editOptions);
      await submitAddressForm(frame, EXPECTED_ADDRESS, editOptions);

      await spawnPaymentDialogTask(
        frame,
        async address => {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              let addresses = Object.entries(state.savedAddresses);
              return (
                addresses.length == 1 &&
                addresses[0][1]["given-name"] == address["given-name"]
              );
            },
            "Check address was edited"
          );

          let addressGUIDs = Object.keys(state.savedAddresses);
          is(addressGUIDs.length, 1, "Check there is still one address");
          let savedAddress = state.savedAddresses[addressGUIDs[0]];
          for (let [key, val] of Object.entries(address)) {
            is(savedAddress[key], val, "Check updated " + key);
          }
          ok(savedAddress.guid, "Address has a guid");
          ok(savedAddress.name, "Address has a name");
          ok(
            savedAddress.name.includes(address["given-name"]) &&
              savedAddress.name.includes(address["family-name"]),
            "Address.name was computed"
          );

          state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return state.page.id == "payment-summary";
            },
            "Switched back to payment-summary"
          );
        },
        EXPECTED_ADDRESS
      );

      await shippingAddressChangePromise;
      info("got shippingaddresschange event");

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

add_task(async function test_add_payer_contact_name_email_link() {
  await setup();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestPayerNameAndEmail,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      const EXPECTED_ADDRESS = {
        "given-name": "Deraj",
        "family-name": "Niew",
        email: "test@example.com",
      };

      const addOptions = {
        addLinkSelector: "address-picker.payer-related .add-link",
        checkboxSelector: "#payer-address-page .persist-checkbox",
        initialPageId: "payment-summary",
        addressPageId: "payer-address-page",
        expectPersist: true,
      };

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let {
          tempAddresses,
          savedAddresses,
        } = await PTU.DialogContentUtils.getCurrentState(content);
        is(
          Object.keys(tempAddresses).length,
          0,
          "No temporary addresses at the start of test"
        );
        is(
          Object.keys(savedAddresses).length,
          1,
          "1 saved address at the start of test"
        );
      });

      await navigateToAddAddressPage(frame, addOptions);

      await spawnPaymentDialogTask(frame, async () => {
        let addressForm = content.document.querySelector("#payer-address-page");
        ok(content.isVisible(addressForm), "Payer address form is visible");

        let title = addressForm.querySelector("address-form h2");
        is(title.textContent, "Add Payer Contact", "Page title should be set");

        let saveButton = addressForm.querySelector("address-form .save-button");
        is(saveButton.textContent, "Next", "Save button has the correct label");

        info("check that non-payer requested fields are hidden");
        for (let selector of ["#organization", "#tel"]) {
          let element = addressForm.querySelector(selector);
          ok(content.isHidden(element), selector + " should be hidden");
        }
      });

      await fillInPayerAddressForm(frame, EXPECTED_ADDRESS, addOptions);
      await verifyPersistCheckbox(frame, addOptions);
      await submitAddressForm(frame, EXPECTED_ADDRESS, addOptions);

      await spawnPaymentDialogTask(
        frame,
        async address => {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let { savedAddresses } = await PTU.DialogContentUtils.getCurrentState(
            content
          );

          let addressGUIDs = Object.keys(savedAddresses);
          is(addressGUIDs.length, 2, "Check there are now 2 addresses");
          let savedAddress = savedAddresses[addressGUIDs[1]];
          for (let [key, val] of Object.entries(address)) {
            is(savedAddress[key], val, "Check " + key);
          }
          ok(savedAddress.guid, "Address has a guid");
          ok(savedAddress.name, "Address has a name");
          ok(
            savedAddress.name.includes(address["given-name"]) &&
              savedAddress.name.includes(address["family-name"]),
            "Address.name was computed"
          );
        },
        EXPECTED_ADDRESS
      );

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_edit_payer_contact_name_email_phone_link() {
  await setup();
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

      const EXPECTED_ADDRESS = {
        "given-name": "Deraj",
        "family-name": "Niew",
        email: "test@example.com",
        tel: "+15555551212",
      };
      const editOptions = {
        checkboxSelector: "#payer-address-page .persist-checkbox",
        initialPageId: "payment-summary",
        addressPageId: "payer-address-page",
        expectPersist: true,
        isEditing: true,
      };

      await spawnPaymentDialogTask(
        frame,
        async address => {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return Object.keys(state.savedAddresses).length == 1;
            },
            "One saved addresses when starting test"
          );

          let editLink = content.document.querySelector(
            "address-picker.payer-related .edit-link"
          );
          is(editLink.textContent, "Edit", "Edit link text");

          editLink.click();

          await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return (
                state.page.id == "payer-address-page" &&
                !!state["payer-address-page"].guid
              );
            },
            "Check edit page state"
          );

          let addressForm = content.document.querySelector(
            "#payer-address-page"
          );
          ok(content.isVisible(addressForm), "Payer address form is visible");

          let title = addressForm.querySelector("h2");
          is(
            title.textContent,
            "Edit Payer Contact",
            "Page title should be set"
          );

          let saveButton = addressForm.querySelector(".save-button");
          is(
            saveButton.textContent,
            "Update",
            "Save button has the correct label"
          );

          info("check that non-payer requested fields are hidden");
          let formElements = addressForm.querySelectorAll(
            ":-moz-any(input, select, textarea"
          );
          let allowedFields = [
            "given-name",
            "additional-name",
            "family-name",
            "email",
            "tel",
          ];
          for (let element of formElements) {
            let shouldBeVisible = allowedFields.includes(element.id);
            if (shouldBeVisible) {
              ok(content.isVisible(element), element.id + " should be visible");
            } else {
              ok(content.isHidden(element), element.id + " should be hidden");
            }
          }

          info("overwriting field values");
          for (let [key, val] of Object.entries(address)) {
            let field = addressForm.querySelector(`#${key}`);
            field.value = val + "1";
            ok(!field.disabled, `Field #${key} shouldn't be disabled`);
          }
        },
        EXPECTED_ADDRESS
      );

      await verifyPersistCheckbox(frame, editOptions);
      await submitAddressForm(frame, EXPECTED_ADDRESS, editOptions);

      await spawnPaymentDialogTask(
        frame,
        async address => {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              let addresses = Object.entries(state.savedAddresses);
              return (
                addresses.length == 1 &&
                addresses[0][1]["given-name"] == address["given-name"] + "1"
              );
            },
            "Check address was edited"
          );

          let addressGUIDs = Object.keys(state.savedAddresses);
          is(addressGUIDs.length, 1, "Check there is still one address");
          let savedAddress = state.savedAddresses[addressGUIDs[0]];
          for (let [key, val] of Object.entries(address)) {
            is(savedAddress[key], val + "1", "Check updated " + key);
          }
          ok(savedAddress.guid, "Address has a guid");
          ok(savedAddress.name, "Address has a name");
          ok(
            savedAddress.name.includes(address["given-name"]) &&
              savedAddress.name.includes(address["family-name"]),
            "Address.name was computed"
          );
        },
        EXPECTED_ADDRESS
      );

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_shipping_address_picker() {
  await setup();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: BLANK_PAGE_URL,
    },
    async browser => {
      let { win, frame } = await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      });

      await spawnPaymentDialogTask(
        frame,
        async function test_picker_option_label(address) {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );
          const { FormAutofillUtils } = ChromeUtils.import(
            "resource://formautofill/FormAutofillUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return Object.keys(state.savedAddresses).length == 1;
            },
            "One saved addresses when starting test"
          );
          let savedAddress = Object.values(state.savedAddresses)[0];

          let selector =
            "address-picker[selected-state-key='selectedShippingAddress']";
          let picker = content.document.querySelector(selector);
          let option = Cu.waiveXrays(picker).dropdown.popupBox.children[0];
          is(
            option.textContent,
            FormAutofillUtils.getAddressLabel(savedAddress, null),
            "Shows correct shipping option label"
          );
        }
      );

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

add_task(async function test_payer_address_picker() {
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
        async function test_picker_option_label(address) {
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );
          const { FormAutofillUtils } = ChromeUtils.import(
            "resource://formautofill/FormAutofillUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return Object.keys(state.savedAddresses).length == 1;
            },
            "One saved addresses when starting test"
          );
          let savedAddress = Object.values(state.savedAddresses)[0];

          let selector =
            "address-picker[selected-state-key='selectedPayerAddress']";
          let picker = content.document.querySelector(selector);
          let option = Cu.waiveXrays(picker).dropdown.popupBox.children[0];
          is(
            option.textContent.includes("32 Vassar Street"),
            false,
            "Payer option label does not contain street address"
          );
          is(
            option.textContent,
            FormAutofillUtils.getAddressLabel(savedAddress, "name tel email"),
            "Shows correct payer option label"
          );
        }
      );

      info("clicking cancel");
      spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

      await BrowserTestUtils.waitForCondition(
        () => win.closed,
        "dialog should be closed"
      );
    }
  );
});

/*
 * Test that we can correctly add an address from a private window
 */
add_task(async function test_private_persist_addresses() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(false, "Cannot test OS key store login on official builds.");
    return;
  }
  let prefilledGuids = await setup();

  is(
    (await formAutofillStorage.addresses.getAll()).length,
    1,
    "Setup results in 1 stored address at start of test"
  );

  await withNewTabInPrivateWindow(
    {
      url: BLANK_PAGE_URL,
    },
    async browser => {
      info("in new tab w. private window");
      let { frame } =
        // setupPaymentDialog from a private window.
        await setupPaymentDialog(browser, {
          methodData: [PTU.MethodData.basicCard],
          details: Object.assign(
            {},
            PTU.Details.twoShippingOptions,
            PTU.Details.total2USD
          ),
          options: PTU.Options.requestShippingOption,
          merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
        });
      info("/setupPaymentDialog");

      let addressToAdd = Object.assign({}, PTU.Addresses.Temp);
      // Emails aren't part of shipping addresses
      delete addressToAdd.email;
      const addOptions = {
        addLinkSelector: "address-picker.shipping-related .add-link",
        checkboxSelector: "#shipping-address-page .persist-checkbox",
        initialPageId: "payment-summary",
        addressPageId: "shipping-address-page",
        expectPersist: false,
        isPrivate: true,
      };

      await navigateToAddAddressPage(frame, addOptions);
      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let state = await PTU.DialogContentUtils.getCurrentState(content);
        info("on address-page and state.isPrivate: " + state.isPrivate);
        ok(
          state.isPrivate,
          "isPrivate flag is set when paymentrequest is shown in a private session"
        );
      });

      info("wait for initialAddresses");
      let initialAddresses = await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.getShippingAddresses
      );
      is(
        initialAddresses.options.length,
        1,
        "Got expected number of pre-filled shipping addresses"
      );

      await fillInShippingAddressForm(frame, addressToAdd, addOptions);
      await verifyPersistCheckbox(frame, addOptions);
      await submitAddressForm(frame, addressToAdd, addOptions);

      let shippingAddresses = await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.getShippingAddresses
      );
      info("shippingAddresses", shippingAddresses);
      let addressOptions = shippingAddresses.options;
      // expect the prefilled address + the new temporary address
      is(
        addressOptions.length,
        2,
        "The picker has the expected number of address options"
      );
      let tempAddressOption = addressOptions.find(
        addr => addr.guid != prefilledGuids.address1GUID
      );
      let tempAddressGuid = tempAddressOption.guid;
      // select the new address
      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.selectShippingAddressByGuid,
        tempAddressGuid
      );

      info("awaiting the shippingaddresschange event");
      await ContentTask.spawn(
        browser,
        {
          eventName: "shippingaddresschange",
        },
        PTU.ContentTasks.awaitPaymentEventPromise
      );

      await spawnPaymentDialogTask(
        frame,
        async args => {
          let { address, tempAddressGuid, prefilledGuids: guids } = args;
          let { PaymentTestUtils: PTU } = ChromeUtils.import(
            "resource://testing-common/PaymentTestUtils.jsm"
          );

          let state = await PTU.DialogContentUtils.waitForState(
            content,
            state => {
              return state.selectedShippingAddress == tempAddressGuid;
            },
            "Check the temp address is the selectedShippingAddress"
          );

          let addressGUIDs = Object.keys(state.tempAddresses);
          is(addressGUIDs.length, 1, "Check there is one address");

          is(
            addressGUIDs[0],
            tempAddressGuid,
            "guid from state and picker options match"
          );
          let tempAddress = state.tempAddresses[tempAddressGuid];
          for (let [key, val] of Object.entries(address)) {
            is(tempAddress[key], val, "Check field " + key);
          }
          ok(tempAddress.guid, "Address has a guid");
          ok(tempAddress.name, "Address has a name");
          ok(
            tempAddress.name.includes(address["given-name"]) &&
              tempAddress.name.includes(address["family-name"]),
            "Address.name was computed"
          );

          let paymentMethodPicker = content.document.querySelector(
            "payment-method-picker"
          );
          content.fillField(
            Cu.waiveXrays(paymentMethodPicker).dropdown.popupBox,
            guids.card1GUID
          );
        },
        { address: addressToAdd, tempAddressGuid, prefilledGuids }
      );

      await spawnPaymentDialogTask(
        frame,
        PTU.DialogContentTasks.setSecurityCode,
        {
          securityCode: "123",
        }
      );

      info("clicking pay");
      await loginAndCompletePayment(frame);

      // Add a handler to complete the payment above.
      info("acknowledging the completion from the merchant page");
      let result = await ContentTask.spawn(
        browser,
        {},
        PTU.ContentTasks.addCompletionHandler
      );

      // Verify response has the expected properties
      info("response: " + JSON.stringify(result.response));
      let responseAddress = result.response.shippingAddress;
      ok(responseAddress, "response should contain the shippingAddress");
      ok(
        responseAddress.recipient.includes(addressToAdd["given-name"]),
        "Check given-name matches recipient in response"
      );
      ok(
        responseAddress.recipient.includes(addressToAdd["family-name"]),
        "Check family-name matches recipient in response"
      );
      is(
        responseAddress.addressLine[0],
        addressToAdd["street-address"],
        "Check street-address in response"
      );
      is(
        responseAddress.country,
        addressToAdd.country,
        "Check country in response"
      );
    }
  );
  // verify formautofill store doesnt have the new temp addresses
  is(
    (await formAutofillStorage.addresses.getAll()).length,
    1,
    "Original 1 stored address at end of test"
  );
});

add_task(async function test_countrySpecificFieldsGetRequiredness() {
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

      let addOptions = {
        addLinkSelector: "address-picker.shipping-related .add-link",
        checkboxSelector: "#shipping-address-page .persist-checkbox",
        initialPageId: "payment-summary",
        addressPageId: "shipping-address-page",
        expectPersist: true,
      };

      await navigateToAddAddressPage(frame, addOptions);

      const EXPECTED_ADDRESS = {
        country: "MO",
        "given-name": "First",
        "family-name": "Last",
        "street-address": "12345 FooFoo Bar",
      };
      await fillInShippingAddressForm(frame, EXPECTED_ADDRESS, addOptions);
      await submitAddressForm(frame, EXPECTED_ADDRESS, addOptions);

      await navigateToAddAddressPage(frame, addOptions);

      await selectPaymentDialogShippingAddressByCountry(frame, "MO");

      await spawnPaymentDialogTask(frame, async () => {
        let { PaymentTestUtils: PTU } = ChromeUtils.import(
          "resource://testing-common/PaymentTestUtils.jsm"
        );

        let editLink = content.document.querySelector(
          "address-picker.shipping-related .edit-link"
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
        let provinceField = addressForm.querySelector("#address-level1");
        let provinceContainer = provinceField.parentNode;
        is(
          provinceContainer.style.display,
          "none",
          "Province should be hidden for Macau"
        );

        let countryField = addressForm.querySelector("#country");
        await content.fillField(countryField, "CA");
        info("changed selected country to Canada");

        isnot(
          provinceContainer.style.display,
          "none",
          "Province should be visible for Canada"
        );
        ok(
          provinceContainer.hasAttribute("required"),
          "Province container should have required attribute"
        );
        let provinceSpan = provinceContainer.querySelector("span");
        is(
          provinceSpan.getAttribute("fieldRequiredSymbol"),
          "*",
          "Province span should have asterisk as fieldRequiredSymbol"
        );
        is(
          content.window.getComputedStyle(provinceSpan, "::after").content,
          "attr(fieldRequiredSymbol)",
          "Asterisk should be on Province"
        );

        let addressBackButton = addressForm.querySelector(".back-button");
        addressBackButton.click();

        await PTU.DialogContentUtils.waitForState(
          content,
          state => {
            return state.page.id == "payment-summary";
          },
          "Switched back to payment-summary"
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
