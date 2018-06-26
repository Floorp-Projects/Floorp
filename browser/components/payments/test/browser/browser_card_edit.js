/* eslint-disable no-shadow */

"use strict";

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

    await navigateToAddCardPage(frame);
    await verifyPersistCheckbox(frame, {
      checkboxSelector: "basic-card-form .persist-checkbox",
      expectPersist: !aOptions.isPrivate,
    });
    await spawnPaymentDialogTask(frame, async (testArgs = {}) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return Object.keys(state.savedBasicCards).length == 1 &&
               Object.keys(state.savedAddresses).length == 1;
      }, "Check no cards or addresses present at beginning of test");

      let title = content.document.querySelector("basic-card-form h2");
      is(title.textContent, "Add Credit Card", "Add title should be set");

      is(state.isPrivate, testArgs.isPrivate,
         "isPrivate flag has expected value when shown from a private/non-private session");
    }, aOptions);

    await fillInCardForm(frame, PTU.BasicCards.JaneMasterCard, {
      isTemporary: aOptions.isPrivate,
      checkboxSelector: "basic-card-form .persist-checkbox",
    });

    await spawnPaymentDialogTask(frame, async (testArgs = {}) => {
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
      expectPersist: !aOptions.isPrivate,
    });
    await navigateToAddAddressPage(frame, addressOptions);

    await spawnPaymentDialogTask(frame, async (testArgs = {}) => {
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
      }, "Check basic-card page, but card should not be saved and no addresses present");

      is(title.textContent, "Add Credit Card", "Add title should be still be on credit card page");

      for (let [key, val] of Object.entries(card)) {
        let field = content.document.getElementById(key);
        is(field.value, val, "Field should still have previous value entered");
        ok(!field.disabled, "Fields should still be enabled for editing");
      }
    }, aOptions);

    await navigateToAddAddressPage(frame, addressOptions);

    await fillInAddressForm(frame, PTU.Addresses.TimBL2, addressOptions);

    await verifyPersistCheckbox(frame, addressOptions);

    await spawnPaymentDialogTask(frame, async (testArgs = {}) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      content.document.querySelector("address-form button:last-of-type").click();

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "basic-card-page" && !state["basic-card-page"].guid;
      }, "Check address was added and we're back on basic-card page (add)");

      let addressCount = Object.keys(state.savedAddresses).length +
                         Object.keys(state.tempAddresses).length;
      is(addressCount, 2, "Check address was added");

      let addressColn = testArgs.isPrivate ? state.tempAddresses : state.savedAddresses;

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
      if (testArgs.isPrivate) {
        // guid property is added in later patch on bug 1463608
        is(selectedAddressGuid, lastAddress.guid,
           "The select should have the new address selected");
      } else {
        is(selectedAddressGuid, lastAddress.guid,
           "The select should have the new address selected");
      }
    }, aOptions);

    await fillInCardForm(frame, PTU.BasicCards.JaneMasterCard, {
      isTemporary: aOptions.isPrivate,
      checkboxSelector: "basic-card-form .persist-checkbox",
    });

    await spawnPaymentDialogTask(frame, async (testArgs = {}) => {
      let {
        PaymentTestUtils: PTU,
      } = ChromeUtils.import("resource://testing-common/PaymentTestUtils.jsm", {});

      let {prefilledGuids} = testArgs;
      let card = Object.assign({}, PTU.BasicCards.JaneMasterCard);

      content.document.querySelector("basic-card-form button:last-of-type").click();

      let state = await PTU.DialogContentUtils.waitForState(content, (state) => {
        return state.page.id == "payment-summary";
      }, "Check we are back on the sumamry page");

      let cardCount = Object.keys(state.savedBasicCards).length +
                         Object.keys(state.tempBasicCards).length;
      is(cardCount, 2, "Card was added");
      if (testArgs.isPrivate) {
        is(Object.keys(state.tempBasicCards).length, 1, "Card was added temporarily");
        is(Object.keys(state.savedBasicCards).length, 1, "No change to saved cards");
      } else {
        is(Object.keys(state.tempBasicCards).length, 0, "No temporary cards addded");
        is(Object.keys(state.savedBasicCards).length, 2, "New card was saved");
      }

      let cardCollection = testArgs.isPrivate ? state.tempBasicCards : state.savedBasicCards;
      let addressCollection = testArgs.isPrivate ? state.tempAddresses : state.savedAddresses;
      let savedCardGUID =
        Object.keys(cardCollection).find(key => key != prefilledGuids.card1GUID);
      let savedAddressGUID =
        Object.keys(addressCollection).find(key => key != prefilledGuids.address1GUID);
      let savedCard = savedCardGUID && cardCollection[savedCardGUID];

      card["cc-number"] = "************4444"; // Card should be masked

      for (let [key, val] of Object.entries(card)) {
        if (key == "cc-number" && testArgs.isPrivate) {
          // cc-number is not yet masked for private/temporary cards
          is(savedCard[key], val, "Check " + key);
        } else {
          is(savedCard[key], val, "Check " + key);
        }
      }
      if (testArgs.isPrivate) {
        ok(testArgs.isPrivate,
           "Checking card/address from private window relies on guid property " +
           "which isnt available yet");
      } else {
        is(savedCard.billingAddressGUID, savedAddressGUID,
           "The saved card should be associated with the billing address");
      }
    }, aOptions);

    spawnPaymentDialogTask(frame, PTU.DialogContentTasks.manuallyClickCancel);
    await BrowserTestUtils.waitForCondition(() => win.closed, "dialog should be closed");
  });
}

add_task(async function test_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  await add_link({
    isPrivate: false,
    prefilledGuids,
  });
}).skip();

add_task(async function test_private_add_link() {
  let prefilledGuids = await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
  await add_link({
    isPrivate: true,
    prefilledGuids,
  });
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
}).skip();

add_task(async function test_private_card_adding() {
  await setup([PTU.Addresses.TimBL], [PTU.BasicCards.JohnDoe]);
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
  }, args, {
    browser: privateWin.gBrowser,
  });
  await BrowserTestUtils.closeWindow(privateWin);
}).skip();
