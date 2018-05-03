/* eslint-disable no-shadow */

"use strict";

add_task(async function test_add_link() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: PTU.Details.twoShippingOptions,
        options: PTU.Options.requestShippingOption,
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );

    let shippingAddressChangePromise = ContentTask.spawn(browser, {
      eventName: "shippingaddresschange",
    }, PTU.ContentTasks.awaitPaymentRequestEventPromise);

    const EXPECTED_ADDRESS = {
      "given-name": "Jared",
      "family-name": "Wein",
      "organization": "Mozilla",
      "street-address": "404 Internet Lane",
      "address-level2": "Firefoxity City",
      "address-level1": "CA",
      "postal-code": "31337",
      "country": "US",
      "tel": "+15555551212",
      "email": "test@example.com",
    };

    await spawnPaymentDialogTask(frame, async (address) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedAddresses).length == 0;
      }, "No saved addresses when starting test");

      let addLink = content.document.querySelector("address-picker a");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state.page.guid;
      }, "Check add page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Add Shipping Address", "Page title should be set");

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
    }, EXPECTED_ADDRESS);

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
        details: PTU.Details.twoShippingOptions,
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

      let editLink = content.document.querySelector("address-picker a:nth-of-type(2)");
      is(editLink.textContent, "Edit", "Edit link text");

      editLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !!state.page.guid;
      }, "Check edit page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Edit Shipping Address", "Page title should be set");

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

      let addLink = content.document.querySelector("address-picker.payer-related a");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "address-page" && !state.page.guid;
      }, "Check add page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Add Payer Contact", "Page title should be set");

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
        ok(content.document.querySelector(selector).getBoundingClientRect().height == 0,
           selector + " should be hidden");
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
        content.document.querySelector("address-picker.payer-related a:nth-of-type(2)");
      is(editLink.textContent, "Edit", "Edit link text");

      editLink.click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        info("state.page.id: " + state.page.id + "; state.page.guid: " + state.page.guid);
        return state.page.id == "address-page" && !!state.page.guid;
      }, "Check edit page state");

      let title = content.document.querySelector("address-form h1");
      is(title.textContent, "Edit Payer Contact", "Page title should be set");

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
          ok(element.getBoundingClientRect().height > 0, element.id + " should be visible");
        } else {
          ok(element.getBoundingClientRect().height == 0, element.id + " should be hidden");
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
