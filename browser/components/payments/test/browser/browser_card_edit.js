/* eslint-disable no-shadow */

"use strict";

add_task(async function test_add_link() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let addLink = content.document.querySelector("payment-method-picker .add-link");
    is(addLink.textContent, "Add", "Add link text");

    addLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
    }, "Check add page state");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 0 &&
             Object.keys(state.savedAddresses).length == 0;
    }, "Check no cards or addresses present at beginning of test");

    let title = content.document.querySelector("basic-card-form h1");
    is(title.textContent, "Add Credit Card", "Add title should be set");

    ok(!state.isPrivate,
       "isPrivate flag is not set when paymentrequest is shown from a non-private session");
    let persistCheckbox = content.document.querySelector("basic-card-form labelled-checkbox");
    ok(Cu.waiveXrays(persistCheckbox).checked, "persist checkbox should be checked by default");

    let card = Object.assign({}, PTU.BasicCards.JohnDoe);

    info("filling fields");
    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    let billingAddressSelect = content.document.querySelector("#billingAddressGUID");
    ok(content.isVisible(billingAddressSelect),
       "The billing address selector should always be visible");
    is(billingAddressSelect.childElementCount, 1,
       "Only one child option should exist by default");
    is(billingAddressSelect.children[0].value, "",
       "The only option should be the blank/empty option");

    let addressAddLink = content.document.querySelector(".billingAddressRow .add-link");
    addressAddLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && !state["address-page"].guid;
    }, "Check address page state");

    let addressTitle = content.document.querySelector("address-form h1");
    is(addressTitle.textContent, "Add Billing Address",
       "Address on add address page should be correct");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 0;
    }, "Check card was not added when clicking the 'add' address button");

    let addressBackButton = content.document.querySelector("address-form .back-button");
    addressBackButton.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 0;
    }, "Check basic-card page, but card should not be saved and no addresses present");

    is(title.textContent, "Add Credit Card", "Add title should be still be on credit card page");

    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      is(field.value, val, "Field should still have previous value entered");
      ok(!field.disabled, "Fields should still be enabled for editing");
    }

    addressAddLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && !state["address-page"].guid;
    }, "Check address page state");

    info("filling address fields");
    for (let [key, val] of Object.entries(PTU.Addresses.TimBL)) {
      let field = content.document.getElementById(key);
      if (!field) {
        ok(false, `${key} field not found`);
      }
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    content.document.querySelector("address-form button:last-of-type").click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check address was added and we're back on basic-card page (add)");

    ok(state["basic-card-page"].preserveFieldValues,
       "preserveFieldValues should be set when coming back from address-page");

    ok(state["basic-card-page"].billingAddressGUID,
       "billingAddressGUID should be set when coming back from address-page");

    is(billingAddressSelect.childElementCount, 2,
       "Two options should exist in the billingAddressSelect");
    let selectedOption =
      billingAddressSelect.children[billingAddressSelect.selectedIndex];
    let selectedAddressGuid = selectedOption.value;
    is(selectedAddressGuid, Object.values(state.savedAddresses)[0].guid,
       "The select should have the new address selected");

    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      is(field.value, val, `Field #${key} should have value`);
    }

    content.document.querySelector("basic-card-form button:last-of-type").click();

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 1;
    }, "Check card was not added again");

    let cardGUIDs = Object.keys(state.savedBasicCards);
    is(cardGUIDs.length, 1, "Check there is one card");
    let savedCard = state.savedBasicCards[cardGUIDs[0]];
    card["cc-number"] = "************1111"; // Card should be masked
    for (let [key, val] of Object.entries(card)) {
      is(savedCard[key], val, "Check " + key);
    }
    is(savedCard.billingAddressGUID, selectedAddressGuid,
       "The saved card should be associated with the billing address");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "payment-summary";
    }, "Switched back to payment-summary");
  }, args);
});

add_task(async function test_edit_link() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let editLink = content.document.querySelector("payment-method-picker .edit-link");
    is(editLink.textContent, "Edit", "Edit link text");

    editLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && state["basic-card-page"].guid;
    }, "Check edit page state");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 1 &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check card and address present at beginning of test");

    let title = content.document.querySelector("basic-card-form h1");
    is(title.textContent, "Edit Credit Card", "Edit title should be set");

    let card = Object.assign({}, PTU.BasicCards.JohnDoe);
    // cc-number cannot be modified
    delete card["cc-number"];
    card["cc-exp-year"]++;
    card["cc-exp-month"]++;

    info("overwriting field values");
    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }
    ok(content.document.getElementById("cc-number").disabled, "cc-number field should be disabled");

    let billingAddressSelect = content.document.querySelector("#billingAddressGUID");
    is(billingAddressSelect.childElementCount, 2,
       "Two options should exist in the billingAddressSelect");
    is(billingAddressSelect.selectedIndex, 1,
       "The billing address set by the previous test should be selected by default");

    info("Test clicking 'edit' on the empty option first");
    billingAddressSelect.selectedIndex = 0;

    let addressEditLink = content.document.querySelector(".billingAddressRow .edit-link");
    addressEditLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && !state["address-page"].guid;
    }, "Clicking edit button when the empty option is selected will go to 'add' page (no guid)");

    let addressTitle = content.document.querySelector("address-form h1");
    is(addressTitle.textContent, "Add Billing Address",
       "Address on add address page should be correct");

    let addressBackButton = content.document.querySelector("address-form .back-button");
    addressBackButton.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check we're back at basic-card page with no state changed after adding");

    info("Go back to previously selected option before clicking 'edit' now");
    billingAddressSelect.selectedIndex = 1;

    let selectedOption = billingAddressSelect.selectedOptions.length &&
                         billingAddressSelect.selectedOptions[0];
    ok(selectedOption && selectedOption.value, "select should have a selected option value");

    addressEditLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && state["address-page"].guid;
    }, "Check address page state (editing)");

    is(addressTitle.textContent, "Edit Billing Address",
       "Address on edit address page should be correct");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.savedBasicCards).length == 1;
    }, "Check card was not added again when clicking the 'edit' address button");

    addressBackButton.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check we're back at basic-card page with no state changed after editing");

    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      is(field.value, val, "Field should still have previous value entered");
    }

    selectedOption = billingAddressSelect.selectedOptions.length &&
                     billingAddressSelect.selectedOptions[0];
    ok(selectedOption && selectedOption.value, "select should have a selected option value");

    addressEditLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && state["address-page"].guid;
    }, "Check address page state (editing)");

    info("filling address fields");
    for (let [key, val] of Object.entries(PTU.Addresses.TimBL)) {
      let field = content.document.getElementById(key);
      if (!field) {
        ok(false, `${key} field not found`);
      }
      field.value = val + "1";
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    content.document.querySelector("address-form button:last-of-type").click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check still only one address and we're back on basic-card page");

    is(Object.values(state.savedAddresses)[0].tel, PTU.Addresses.TimBL.tel + "1",
       "Check that address was edited and saved");

    content.document.querySelector("basic-card-form button:last-of-type").click();

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      let cards = Object.entries(state.savedBasicCards);
      return cards.length == 1 &&
             cards[0][1]["cc-name"] == card["cc-name"];
    }, "Check card was edited");

    let cardGUIDs = Object.keys(state.savedBasicCards);
    is(cardGUIDs.length, 1, "Check there is still one card");
    let savedCard = state.savedBasicCards[cardGUIDs[0]];
    is(savedCard["cc-number"], "************1111", "Card number should be masked and unmodified.");
    for (let [key, val] of Object.entries(card)) {
      is(savedCard[key], val, "Check updated " + key);
    }

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "payment-summary";
    }, "Switched back to payment-summary");
  }, args);
});

add_task(async function test_private_persist_defaults() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let addLink = content.document.querySelector("payment-method-picker .add-link");
    is(addLink.textContent, "Add", "Add link text");

    addLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
    },
                                                          "Check add page state");

    ok(!state.isPrivate,
       "isPrivate flag is not set when paymentrequest is shown from a non-private session");
    let persistCheckbox = content.document.querySelector("basic-card-form labelled-checkbox");
    ok(Cu.waiveXrays(persistCheckbox).checked,
       "checkbox is checked by default from a non-private session");
  }, args);

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let addLink = content.document.querySelector("payment-method-picker .add-link");
    is(addLink.textContent, "Add", "Add link text");

    addLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
    },
                                                          "Check add page state");

    ok(state.isPrivate,
       "isPrivate flag is set when paymentrequest is shown from a private session");
    let persistCheckbox = content.document.querySelector("labelled-checkbox");
    ok(!Cu.waiveXrays(persistCheckbox).checked,
       "checkbox is not checked by default from a private session");
  }, args, {
    browser: privateWin.gBrowser,
  });
  await BrowserTestUtils.closeWindow(privateWin);
});

add_task(async function test_private_card_adding() {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, async function check() {
    let {
      PaymentTestUtils: PTU,
    } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

    let addLink = content.document.querySelector("payment-method-picker .add-link");
    is(addLink.textContent, "Add", "Add link text");

    addLink.click();

    let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
    },
                                                          "Check add page state");

    let savedCardCount = Object.keys(state.savedBasicCards).length;
    let tempCardCount = Object.keys(state.tempBasicCards).length;

    let card = Object.assign({}, PTU.BasicCards.JohnDoe);

    info("filling fields");
    for (let [key, val] of Object.entries(card)) {
      let field = content.document.getElementById(key);
      field.value = val;
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    content.document.querySelector("basic-card-form button:last-of-type").click();

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return Object.keys(state.tempBasicCards).length > tempCardCount;
    },
                                                      "Check card was added to temp collection");

    is(savedCardCount, Object.keys(state.savedBasicCards).length, "No card was saved in state");
    is(Object.keys(state.tempBasicCards).length, 1, "Card was added temporarily");
  }, args, {
    browser: privateWin.gBrowser,
  });
  await BrowserTestUtils.closeWindow(privateWin);
});
