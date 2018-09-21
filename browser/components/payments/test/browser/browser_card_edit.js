/* eslint-disable no-shadow */

"use strict";

requestLongerTimeout(2);

async function setup(addresses = [], cards = []) {
  await setupFormAutofillStorage();
  await cleanupFormAutofillStorage();
  let prefilledGuids = await addSampleAddressesAndBasicCard(addresses, cards);
  return prefilledGuids;
}

async function add_link(aOptions = {}) {
  let tabOpenFn = aOptions.isPrivate ? withNewTabInPrivateWindow : BrowserTestUtils.withNewTab;
  await tabOpenFn({
    gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} =
      await setupPaymentDialog(browser, {
        methodData: [PTU.MethodData.basicCard],
        details: Object.assign({}, PTU.Details.total60USD),
        merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
      }
    );
    info("add_link, aOptions: " + JSON.stringify(aOptions, null, 2));
    await navigateToAddCardPage(frame);
    info(`add_link, from the add card page,
          verifyPersistCheckbox with expectPersist: ${aOptions.expectDefaultCardPersist}`);
    await verifyPersistCheckbox(frame, {
      checkboxSelector: "basic-card-form .persist-checkbox",
      expectPersist: aOptions.expectDefaultCardPersist,
    });
    await spawnPaymentDialogTask(frame, async function checkState(testArgs = {}) {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedBasicCards).length == 1 &&
               Object.keys(state.savedAddresses).length == 1;
      }, "Check no cards or addresses present at beginning of test");

      let title = content.document.querySelector("basic-card-form h2");
      is(title.textContent, "Add Credit Card", "Add title should be set");

      let saveButton = content.document.querySelector("basic-card-form .save-button");
      is(saveButton.textContent, "Add", "Save button has the correct label");

      is(state.isPrivate, testArgs.isPrivate,
         "isPrivate flag has expected value when shown from a private/non-private session");
    }, aOptions);

    let cardOptions = Object.assign({}, {
      checkboxSelector: "basic-card-form .persist-checkbox",
      expectPersist: aOptions.expectCardPersist,
      networkSelector: "basic-card-form #cc-type",
      expectedNetwork: PTU.BasicCards.JaneMasterCard["cc-type"],
    });
    if (aOptions.hasOwnProperty("setCardPersistCheckedValue")) {
      cardOptions.setPersistCheckedValue = aOptions.setCardPersistCheckedValue;
    }
    await fillInCardForm(frame, PTU.BasicCards.JaneMasterCard, cardOptions);

    await verifyCardNetwork(frame, cardOptions);
    await verifyPersistCheckbox(frame, cardOptions);

    await spawnPaymentDialogTask(frame, async function checkBillingAddressPicker(testArgs = {}) {
      let billingAddressSelect = content.document.querySelector("#billingAddressGUID");
      ok(content.isVisible(billingAddressSelect),
         "The billing address selector should always be visible");
      is(billingAddressSelect.childElementCount, 2,
         "Only 2 child options should exist by default");
      is(billingAddressSelect.children[0].value, "",
         "The first option should be the blank/empty option");
      ok(billingAddressSelect.children[1].value, "",
         "The 2nd option is the prefilled address and should be truthy");
    }, aOptions);

    let addressOptions = Object.assign({}, aOptions, {
      addLinkSelector: ".billingAddressRow .add-link",
      checkboxSelector: "#address-page .persist-checkbox",
      initialPageId: "basic-card-page",
      expectPersist: aOptions.expectDefaultAddressPersist,
    });
    if (aOptions.hasOwnProperty("setAddressPersistCheckedValue")) {
      addressOptions.setPersistCheckedValue = aOptions.setAddressPersistCheckedValue;
    }

    await navigateToAddAddressPage(frame, addressOptions);

    await spawnPaymentDialogTask(frame, async function checkTask(testArgs = {}) {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let title = content.document.querySelector("basic-card-form h2");
      let card = Object.assign({}, PTU.BasicCards.JaneMasterCard);

      let addressTitle = content.document.querySelector("address-form h2");
      is(addressTitle.textContent, "Add Billing Address",
         "Address on add address page should be correct");

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        let total = Object.keys(state.savedBasicCards).length +
                    Object.keys(state.tempBasicCards).length;
        return total == 1;
      }, "Check card was not added when clicking the 'add' address button");

      let addressBackButton = content.document.querySelector("address-form .back-button");
      addressBackButton.click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        let total = Object.keys(state.savedBasicCards).length +
                    Object.keys(state.tempBasicCards).length;
        return state.page.id == "basic-card-page" && !state["basic-card-page"].guid &&
               total == 1;
      }, "Check basic-card page, but card should not be saved and no new addresses present");

      is(title.textContent, "Add Credit Card", "Add title should be still be on credit card page");

      for (let [key, val] of Object.entries(card)) {
        let field = content.document.getElementById(key);
        is(field.value, val, "Field should still have previous value entered");
        ok(!field.disabled, "Fields should still be enabled for editing");
      }
    }, aOptions);

    await navigateToAddAddressPage(frame, addressOptions);

    await fillInBillingAddressForm(frame, PTU.Addresses.TimBL2, addressOptions);

    await verifyPersistCheckbox(frame, addressOptions);

    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.clickPrimaryButton);

    await spawnPaymentDialogTask(frame, async function checkCardPage(testArgs = {}) {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
      }, "Check address was added and we're back on basic-card page (add)");

      let addressCount = Object.keys(state.savedAddresses).length +
                         Object.keys(state.tempAddresses).length;
      is(addressCount, 2, "Check address was added");

      let addressColn = testArgs.expectAddressPersist ? state.savedAddresses : state.tempAddresses;

      ok(state["basic-card-page"].preserveFieldValues,
         "preserveFieldValues should be set when coming back from address-page");

      ok(state["basic-card-page"].billingAddressGUID,
         "billingAddressGUID should be set when coming back from address-page");

      let billingAddressSelect = content.document.querySelector("#billingAddressGUID");

      is(billingAddressSelect.childElementCount, 3,
         "Three options should exist in the billingAddressSelect");
      let selectedOption =
        billingAddressSelect.children[billingAddressSelect.selectedIndex];
      let selectedAddressGuid = selectedOption.value;
      let lastAddress = Object.values(addressColn)[Object.keys(addressColn).length - 1];
      is(selectedAddressGuid, lastAddress.guid, "The select should have the new address selected");
    }, aOptions);

    cardOptions = Object.assign({}, {
      checkboxSelector: "basic-card-form .persist-checkbox",
      expectPersist: aOptions.expectCardPersist,
    });

    await verifyPersistCheckbox(frame, cardOptions);

    await spawnPaymentDialogTask(frame, async function checkSaveButtonUpdatesOnCCNumberChange() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let button = content.document.querySelector(`basic-card-form button.primary`);
      ok(!button.disabled, "Save button should not be disabled");

      let field = content.document.getElementById("cc-number");
      field.focus();
      EventUtils.sendString("a", content.window);
      button.focus();

      ok(button.disabled, "Save button should be disabled with incorrect number");

      field.focus();
      content.fillField(field, PTU.BasicCards.JaneMasterCard["cc-number"]);
      button.focus();

      ok(!button.disabled, "Save button should be enabled with correct number");
    });

    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.clickPrimaryButton);

    await spawnPaymentDialogTask(frame, async function waitForSummaryPage(testArgs = {}) {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Check we are back on the summary page");
    });

    await spawnPaymentDialogTask(frame, PTU.DialogContentTasks.setSecurityCode, {
      securityCode: "123",
    });

    await spawnPaymentDialogTask(frame, async function checkCardState(testArgs = {}) {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let {prefilledGuids} = testArgs;
      let card = Object.assign({}, PTU.BasicCards.JaneMasterCard);
      let state = await PTU.DialogContentUtils.getCurrentState(content);

      let cardCount = Object.keys(state.savedBasicCards).length +
                         Object.keys(state.tempBasicCards).length;
      is(cardCount, 2, "Card was added");
      if (testArgs.expectCardPersist) {
        is(Object.keys(state.tempBasicCards).length, 0, "No temporary cards addded");
        is(Object.keys(state.savedBasicCards).length, 2, "New card was saved");
      } else {
        is(Object.keys(state.tempBasicCards).length, 1, "Card was added temporarily");
        is(Object.keys(state.savedBasicCards).length, 1, "No change to saved cards");
      }

      let cardCollection = testArgs.expectCardPersist ? state.savedBasicCards :
                                                        state.tempBasicCards;
      let addressCollection = testArgs.expectAddressPersist ? state.savedAddresses :
                                                              state.tempAddresses;
      let savedCardGUID =
        Object.keys(cardCollection).find(key => key != prefilledGuids.card1GUID);
      let savedAddressGUID =
        Object.keys(addressCollection).find(key => key != prefilledGuids.address1GUID);
      let savedCard = savedCardGUID && cardCollection[savedCardGUID];

      // we should never have an un-masked cc-number in the state:
      ok(Object.values(cardCollection).every(card => card["cc-number"].startsWith("************")),
         "All cc-numbers in state are masked");
      card["cc-number"] = "************4444"; // Expect card number to be masked at this point
      for (let [key, val] of Object.entries(card)) {
        is(savedCard[key], val, "Check " + key);
      }

      is(savedCard.billingAddressGUID, savedAddressGUID,
         "The saved card should be associated with the billing address");
    }, aOptions);

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.completePayment);

    // Add a handler to complete the payment above.
    info("acknowledging the completion from the merchant page");
    let result = await ContentTask.spawn(browser, {}, PTU.ContentTasks.addCompletionHandler);

    // Verify response has the expected properties
    let expectedDetails = Object.assign({
      "cc-security-code": "123",
    }, PTU.BasicCards.JaneMasterCard);
    let expectedBillingAddress = PTU.Addresses.TimBL2;
    let cardDetails = result.response.details;

    checkPaymentMethodDetailsMatchesCard(cardDetails, expectedDetails,
                                         "Check response payment details");
    checkPaymentAddressMatchesStorageAddress(cardDetails.billingAddress, expectedBillingAddress,
                                             "Check response billing address");

    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
}

add_task(async function test_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  let defaultPersist = Services.prefs.getBoolPref(SAVE_CREDITCARD_DEFAULT_PREF);

  is(defaultPersist, false, `Expect ${SAVE_CREDITCARD_DEFAULT_PREF} to default to false`);
  info("Calling add_link from test_add_link");
  await add_link({
    isPrivate: false,
    expectDefaultCardPersist: false,
    expectCardPersist: false,
    expectDefaultAddressPersist: true,
    expectAddressPersist: true,
    prefilledGuids,
  });
});

add_task(async function test_private_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  info("Calling add_link from test_private_add_link");
  await add_link({
    isPrivate: true,
    expectDefaultCardPersist: false,
    expectCardPersist: false,
    expectDefaultAddressPersist: false,
    expectAddressPersist: false,
    prefilledGuids,
  });
});

add_task(async function test_persist_prefd_on_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  Services.prefs.setBoolPref(SAVE_CREDITCARD_DEFAULT_PREF, true);

  info("Calling add_link from test_persist_prefd_on_add_link");
  await add_link({
    isPrivate: false,
    expectDefaultCardPersist: true,
    expectCardPersist: true,
    expectDefaultAddressPersist: true,
    expectAddressPersist: true,
    prefilledGuids,
  });
  Services.prefs.clearUserPref(SAVE_CREDITCARD_DEFAULT_PREF);
});

add_task(async function test_private_persist_prefd_on_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  Services.prefs.setBoolPref(SAVE_CREDITCARD_DEFAULT_PREF, true);

  info("Calling add_link from test_private_persist_prefd_on_add_link");
  // in private window, even when the pref is set true,
  // we should still default to not saving credit-card info
  await add_link({
    isPrivate: true,
    expectDefaultCardPersist: false,
    expectCardPersist: false,
    expectDefaultAddressPersist: false,
    expectAddressPersist: false,
    prefilledGuids,
  });
  Services.prefs.clearUserPref(SAVE_CREDITCARD_DEFAULT_PREF);
});

add_task(async function test_optin_persist_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  let defaultPersist = Services.prefs.getBoolPref(SAVE_CREDITCARD_DEFAULT_PREF);

  is(defaultPersist, false, `Expect ${SAVE_CREDITCARD_DEFAULT_PREF} to default to false`);
  info("Calling add_link from test_add_link");
  // verify that explicit opt-in by checking the box results in the record being saved
  await add_link({
    isPrivate: false,
    expectDefaultCardPersist: false,
    setCardPersistCheckedValue: true,
    expectCardPersist: true,
    expectDefaultAddressPersist: true,
    expectAddressPersist: true,
    prefilledGuids,
  });
});

add_task(async function test_optin_private_persist_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  let defaultPersist = Services.prefs.getBoolPref(SAVE_CREDITCARD_DEFAULT_PREF);

  is(defaultPersist, false, `Expect ${SAVE_CREDITCARD_DEFAULT_PREF} to default to false`);
  // verify that explicit opt-in for the card only from a private window results
  // in the record being saved
  await add_link({
    isPrivate: true,
    expectDefaultCardPersist: false,
    setCardPersistCheckedValue: true,
    expectCardPersist: true,
    expectDefaultAddressPersist: false,
    expectAddressPersist: false,
    prefilledGuids,
  });
});

add_task(async function test_opt_out_persist_prefd_on_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  Services.prefs.setBoolPref(SAVE_CREDITCARD_DEFAULT_PREF, true);

  // set the pref to default to persist creditcards, but manually uncheck the checkbox
  await add_link({
    isPrivate: false,
    expectDefaultCardPersist: true,
    setCardPersistCheckedValue: false,
    expectCardPersist: false,
    expectDefaultAddressPersist: true,
    expectAddressPersist: true,
    prefilledGuids,
  });
  Services.prefs.clearUserPref(SAVE_CREDITCARD_DEFAULT_PREF);
});

add_task(async function test_edit_link() {
  // add an address and card linked to this address
  let prefilledGuids = await setup([PTU.Addresses.TimBL]);
  {
    let card = Object.assign({}, PTU.BasicCards.JohnDoe,
                             { billingAddressGUID: prefilledGuids.address1GUID });
    await addCardRecord(card);
  }

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

    let title = content.document.querySelector("basic-card-form h2");
    is(title.textContent, "Edit Credit Card", "Edit title should be set");

    let saveButton = content.document.querySelector("basic-card-form .save-button");
    is(saveButton.textContent, "Update", "Save button has the correct label");

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
       "The prefilled billing address should be selected by default");

    info("Test clicking 'edit' on the empty option first");
    billingAddressSelect.selectedIndex = 0;

    let addressEditLink = content.document.querySelector(".billingAddressRow .edit-link");
    addressEditLink.click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "address-page" && !state["address-page"].guid;
    }, "Clicking edit button when the empty option is selected will go to 'add' page (no guid)");

    let addressTitle = content.document.querySelector("address-form h2");
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

    info("modify some address fields");
    for (let key of ["given-name", "tel", "organization", "street-address"]) {
      let field = content.document.getElementById(key);
      if (!field) {
        ok(false, `${key} field not found`);
      }
      field.focus();
      EventUtils.sendKey("BACK_SPACE", content.window);
      EventUtils.sendString("7", content.window);
      ok(!field.disabled, `Field #${key} shouldn't be disabled`);
    }

    content.document.querySelector("address-form button.save-button").click();
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "basic-card-page" && state["basic-card-page"].guid &&
             Object.keys(state.savedAddresses).length == 1;
    }, "Check still only one address and we're back on basic-card page");

    is(Object.values(state.savedAddresses)[0].tel, PTU.Addresses.TimBL.tel.slice(0, -1) + "7",
       "Check that address was edited and saved");

    content.document.querySelector("basic-card-form button.save-button").click();

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

add_task(async function test_invalid_network_card_edit() {
  // add an address and card linked to this address
  let prefilledGuids = await setup([PTU.Addresses.TimBL]);
  {
    let card = Object.assign({}, PTU.BasicCards.JohnDoe,
                             { billingAddressGUID: prefilledGuids.address1GUID });
    // create a record with an unknown network id
    card["cc-type"] = "asiv";
    await addCardRecord(card);
  }

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

    let networkSelector = content.document.querySelector("basic-card-form #cc-type");
    todo_is(Cu.waiveXrays(networkSelector).selectedIndex, 0,
            "An invalid cc-type should result in the first option being selected");
    is(Cu.waiveXrays(networkSelector).value, "",
       "An invalid cc-type should result in an empty string as value");

    content.document.querySelector("basic-card-form button.save-button").click();

    // we expect that saving a card with an invalid network will result in the
    // cc-type property being changed to undefined
    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      let cards = Object.entries(state.savedBasicCards);
      return cards.length == 1 &&
             cards[0][1]["cc-type"] == undefined;
    }, "Check card was edited");

    let cardGUIDs = Object.keys(state.savedBasicCards);
    is(cardGUIDs.length, 1, "Check there is still one card");
    let savedCard = state.savedBasicCards[cardGUIDs[0]];
    ok(!savedCard["cc-type"], "We expect the cc-type value to be updated");

    state = await PTU.DialogContentUtils.waitForState(content, (state) => {
      return state.page.id == "payment-summary";
    }, "Switched back to payment-summary");
  }, args);
});

add_task(async function test_private_card_adding() {
  await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  let privateWin = await BrowserTestUtils.openNewBrowserWindow({private: true});

  await BrowserTestUtils.withNewTab({
    gBrowser: privateWin.gBrowser,
    url: BLANK_PAGE_URL,
  }, async browser => {
    let {win, frame} = await setupPaymentDialog(browser, {
      methodData: [PTU.MethodData.basicCard],
      details: PTU.Details.total60USD,
      merchantTaskFn: PTU.ContentTasks.createAndShowRequest,
    });

    await spawnPaymentDialogTask(frame, async function check() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let addLink = content.document.querySelector("payment-method-picker .add-link");
      is(addLink.textContent, "Add", "Add link text");

      addLink.click();

      await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
      },
                                                "Check card page state");
    });

    await fillInCardForm(frame, PTU.BasicCards.JohnDoe);

    await spawnPaymentDialogTask(frame, async function() {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let card = Object.assign({}, PTU.BasicCards.JohnDoe);
      let state = await PTU.DialogContentUtils.getCurrentState(content);
      let savedCardCount = Object.keys(state.savedBasicCards).length;
      let tempCardCount = Object.keys(state.tempBasicCards).length;
      content.document.querySelector("basic-card-form button.save-button").click();

      state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.tempBasicCards).length > tempCardCount;
      },
                                                        "Check card was added to temp collection");

      is(savedCardCount, Object.keys(state.savedBasicCards).length, "No card was saved in state");
      is(Object.keys(state.tempBasicCards).length, 1, "Card was added temporarily");

      let cardGUIDs = Object.keys(state.tempBasicCards);
      is(cardGUIDs.length, 1, "Check there is one card");

      let tempCard = state.tempBasicCards[cardGUIDs[0]];
      // Card number should be masked, so skip cc-number in the compare loop below
      delete card["cc-number"];
      for (let [key, val] of Object.entries(card)) {
        is(tempCard[key], val, "Check " + key + ` ${tempCard[key]} matches ${val}`);
      }
      // check computed fields
      is(tempCard["cc-number"], "************1111", "cc-number is masked");
      is(tempCard["cc-given-name"], "John", "cc-given-name was computed");
      is(tempCard["cc-family-name"], "Doe", "cc-family-name was computed");
      ok(tempCard["cc-exp"], "cc-exp was computed");
      ok(tempCard["cc-number-encrypted"], "cc-number-encrypted was computed");
    });
    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
  await BrowserTestUtils.closeWindow(privateWin);
});

