/* eslint-disable no-shadow */

"use strict";

async function setup() {
  await setupFormAutofillStorage();
  // adds 2 addresses and one card
  await addSampleAddressesAndBasicCard();
}

add_task(async function test_add_link() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 0;
      }, "No saved addresses when starting test");

      let addLink = content.document.querySelector("address-picker .add-link");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state["address-page"].guid;
      }, "Check add page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Add Shipping Address", "Page title should be set");

      let persistCheckbox = content.document.querySelector("address-form labelled-checkbox");
      ok(!persistCheckbox.hidden, "checkbox should be visible when adding a new address");
      ok(Cu.waiveXrays(persistCheckbox).checked, "persist checkbox should be checked by default");

      info("filling fields");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      content.document.querySelector("address-form button:last-of-type").click();
      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "Check address was added");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is one address");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val, "Check " + key);
      }

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, PTU.Addresses.TimBL);

    await shippingAddressChangePromise;
    info("got shippingaddresschange event");

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_edit_link() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    const EXPECTED_ADDRESS = {
      "given-name": "Jaws",
      "family-name": "swaJ",
      "organization": "aliizoM",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "One saved address when starting test");

      let picker = content.document
                     .querySelector("address-picker[selected-state-key='selectedShippingAddress']");
      Cu.waiveXrays(picker).dropdown.click();
      Cu.waiveXrays(picker).dropdown.popupBox.children[0].click();

      let editLink = content.document.querySelector("address-picker .edit-link");
      is(editLink.textContent, "Edit", "Edit link text");

      editLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !!state["address-page"].guid;
      }, "Check edit page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Edit Shipping Address", "Page title should be set");

      let persistCheckbox = content.document.querySelector("address-form labelled-checkbox");
      ok(persistCheckbox.hidden, "checkbox should be hidden when editing an address");

      info("overwriting field values");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      content.document.querySelector("address-form button:last-of-type").click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        let addresses = Object.entries(state.savedAddresses);
        return addresses.length == 1 &&
               addresses[0][1]["given-name"] == address["given-name"];
      }, "Check address was edited");

      // check nothing went into tempAddresses
      is(Object.keys(state.tempAddresses).length, 0, "tempAddresses collection was untouched");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is still one address");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val, "Check updated " + key);
      }

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, EXPECTED_ADDRESS);

    await shippingAddressChangePromise;
    info("got shippingaddresschange event");

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await cleanupFormAutofillStorage();
});

add_task(async function test_add_payer_contact_name_email_link() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestPayerNameAndEmail,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    const EXPECTED_ADDRESS = {
      "given-name": "Deraj",
      "family-name": "Niew",
      "email": "test@example.com",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 0;
      }, "No saved addresses when starting test");

      let addLink = content.document.querySelector("address-picker.payer-related .add-link");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state["address-page"].guid;
      }, "Check add page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Add Payer Contact", "Page title should be set");

      let persistCheckbox = content.document.querySelector("address-form labelled-checkbox");
      ok(!persistCheckbox.hidden, "checkbox should be visible when adding a new address");
      ok(Cu.waiveXrays(persistCheckbox).checked, "persist checkbox should be checked by default");

      info("filling fields");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        if (!field) {
          ok(false, `${key} field not found`);
        }
        field.value = val;
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      info("check that non-payer requested fields are hidden");
      for (let selector of ["#organization", "#tel"]) {
        let element = content.document.querySelector(selector);
        ok(content.isHidden(element), selector + " should be hidden");
      }

      content.document.querySelector("address-form button:last-of-type").click();
      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "Check address was added");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is one addresses");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val, "Check " + key);
      }

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, EXPECTED_ADDRESS);

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_edit_payer_contact_name_email_phone_link() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.total60USD,
        options: PTU.Options.requestPayerNameEmailAndPhone,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    const EXPECTED_ADDRESS = {
      "given-name": "Deraj",
      "family-name": "Niew",
      "email": "test@example.com",
      "tel": "+15555551212",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 1;
      }, "One saved addresses when starting test");

      let editLink =
        content.document.querySelector("address-picker.payer-related .edit-link");
      is(editLink.textContent, "Edit", "Edit link text");

      editLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !!state["address-page"].guid;
      }, "Check edit page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Edit Payer Contact", "Page title should be set");

      let persistCheckbox = content.document.querySelector("address-form labelled-checkbox");
      ok(persistCheckbox.hidden, "checkbox should be hidden when editing an address");

      info("overwriting field values");
      for (let [key, val] of Object.entries(address)) {
        let field = content.document.getElementById(key);
        field.value = val + "1";
        ok(!field.disabled, `Field #${key} shouldn't be disabled`);
      }

      info("check that non-payer requested fields are hidden");
      let formElements =
        content.document.querySelectorAll("address-form :-moz-any(input, select, textarea");
      let allowedFields = ["given-name", "additional-name", "family-name", "email", "tel"];
      for (let element of formElements) {
        let shouldBeVisible = allowedFields.includes(element.id);
        if (shouldBeVisible) {
          ok(content.isVisible(element), element.id + " should be visible");
        } else {
          ok(content.isHidden(element), element.id + " should be hidden");
        }
      }

      content.document.querySelector("address-form button:last-of-type").click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        let addresses = Object.entries(state.savedAddresses);
        return addresses.length == 1 &&
               addresses[0][1]["given-name"] == address["given-name"] + "1";
      }, "Check address was edited");

      let addressGUIDs = Object.keys(state.savedAddresses);
      is(addressGUIDs.length, 1, "Check there is still one address");
      let savedAddress = state.savedAddresses[addressGUIDs[0]];
      for (let [key, val] of Object.entries(address)) {
        is(savedAddress[key], val + "1", "Check updated " + key);
      }

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Switched back to payment-summary");
    }, EXPECTED_ADDRESS);

    info("clicking cancel");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
});

add_task(async function test_private_persist_addresses() {
  formAutofillStorage.addresses.removeAll();
  await setup();

  is((await formAutofillStorage.addresses.getAll()).length, 2,
     "Setup results in 2 stored addresses at start of test");

  await withNewTabInPrivateWindow({
    url: BLANK_PAGE_URL,
  }, async browser => {
    info("in new tab w. private window");
    let {frame} =
      // setupPaymentDialog from a private window.
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.twoShippingOptions, PTU.Details.total2USD),
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );
    info("/setupPaymentDialog");

    await spawnPaymentDialogTask(frame, async () => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      info("adding link");
      // click through to add/edit address page
      let addLink = content.document.querySelector("address-picker a.add-link");
      addLink.click();
      info("link clicked");

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state.page.guid;
      }, "Check add page state");

      info("on address-page and state.isPrivate: " + state.isPrivate);
      ok(state.isPrivate,
         "isPrivate flag is set when paymentrequest is shown in a private session");
      ok(typeof state.isPrivate,
         "isPrivate is a flag");
    });

    info("wait for initialAddresses");
    let initialAddresses =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);
    is(initialAddresses.options.length, 2, "Got expected number of pre-filled shipping addresses");

    // listen for shippingaddresschange event in merchant (private) window
    info("listen for shippingaddresschange");
    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    // add an address
    // (return to summary view)
    info("add an address");
    await spawnPaymentDialogTask(frame, async () => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let persistCheckbox = content.document.querySelector("address-form labelled-checkbox");
      ok(!persistCheckbox.hidden, "checkbox should be visible when adding a new address");
      ok(!Cu.waiveXrays(persistCheckbox).checked,
         "persist checkbox should be unchecked by default");

      info("add the temp address");
      let addressToAdd = PTU.Addresses.Temp;
      for (let [key, val] of Object.entries(addressToAdd)) {
        let field = content.document.getElementById(key);
        field.value = val;
      }
      content.document.querySelector("address-form button:last-of-type").click();

      info("wait until we return to the summary page");
      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Return to summary page");

      is(Object.keys(state.tempAddresses).length, 1, "Address added to temporary collection");
    });

    await shippingAddressChangePromise;
    info("got shippingaddresschange event");

    // verify address picker has more addresses and new selected
    info("check shipping addresses");
    let addresses =
      await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.getShippingAddresses);

    is(addresses.options.length, 3, "Got expected number of shipping addresses");

    info("clicking pay");
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);
    is(result.response.methodName, "basic-card", "Check methodName");
  });
  // verify formautofill store doesnt have the new temp addresses
  is((await formAutofillStorage.addresses.getAll()).length, 2,
     "Original 2 stored addresses at end of test");
});

