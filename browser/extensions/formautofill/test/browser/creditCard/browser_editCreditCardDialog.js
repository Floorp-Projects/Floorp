"use strict";

add_setup(async function() {
  let { formAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  );
  await formAutofillStorage.initialize();
});

add_task(async function test_cancelEditCreditCardDialog() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    win.document.querySelector("#cancel").click();
  });
});

add_task(async function test_cancelEditCreditCardDialogWithESC() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  });
});

add_task(async function test_saveCreditCard() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    ok(
      win.document.documentElement
        .querySelector("title")
        .textContent.includes("Add"),
      "Add card dialog title is correct"
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      "0" + TEST_CREDIT_CARD_1["cc-exp-month"].toString(),
      {},
      win
    );
    is(
      win.document.activeElement.selectedOptions[0].text,
      "04 - April",
      "Displayed month should match number and name"
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD_1["cc-exp-year"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD_1)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[0][fieldName], fieldValue, "check " + fieldName);
  }
  is(creditCards[0].billingAddressGUID, undefined, "check billingAddressGUID");
  ok(creditCards[0]["cc-number-encrypted"], "cc-number-encrypted exists");
});

add_task(async function test_saveCreditCardWithMaxYear() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD_2["cc-exp-month"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD_2["cc-exp-year"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_2["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 2, "Two credit cards are in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD_2)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[1][fieldName], fieldValue, "check " + fieldName);
  }
  ok(creditCards[1]["cc-number-encrypted"], "cc-number-encrypted exists");
  await removeCreditCards([creditCards[1].guid]);
});

add_task(async function test_saveCreditCardWithBillingAddress() {
  await setStorage(TEST_ADDRESS_4, TEST_ADDRESS_1);
  let addresses = await getAddresses();
  let billingAddress = addresses[0];

  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    billingAddressGUID: undefined,
  });

  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD["cc-exp-month"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD["cc-exp-year"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(billingAddress["given-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 2, "Two credit cards are in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[1][fieldName], fieldValue, "check " + fieldName);
  }
  ok(creditCards[1]["cc-number-encrypted"], "cc-number-encrypted exists");
  await removeCreditCards([creditCards[1].guid]);
  await removeAddresses([addresses[0].guid, addresses[1].guid]);
});

add_task(async function test_editCreditCard() {
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      ok(
        win.document.documentElement
          .querySelector("title")
          .textContent.includes("Edit"),
        "Edit card dialog title is correct"
      );
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: creditCards[0],
    }
  );
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD_1["cc-name"] + "test",
    "cc name changed"
  );
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_editCreditCardWithMissingBillingAddress() {
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    billingAddressGUID: "unknown-guid",
  });
  await setStorage(TEST_CREDIT_CARD);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "one credit card in storage");
  is(
    creditCards[0].billingAddressGUID,
    TEST_CREDIT_CARD.billingAddressGUID,
    "Check saved billingAddressGUID"
  );
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: creditCards[0],
    }
  );
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD["cc-name"] + "test",
    "cc name changed"
  );
  is(
    creditCards[0].billingAddressGUID,
    undefined,
    "unknown GUID removed upon manual save"
  );
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_addInvalidCreditCard() {
  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, async win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("test name", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeMouseAtCenter(
      win.document.querySelector("#save"),
      {},
      win
    );

    is(
      win.document.querySelector("form").checkValidity(),
      false,
      "cc-number is invalid"
    );
    await ensureCreditCardDialogNotClosed(win);
    info("closing");
    win.close();
  });
  info("closed");
  let creditCards = await getCreditCards();

  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_editCardWithInvalidNetwork() {
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    "cc-type": "asiv",
  });
  await setStorage(TEST_CREDIT_CARD);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "one credit card in storage");
  is(
    creditCards[0]["cc-type"],
    TEST_CREDIT_CARD["cc-type"],
    "Check saved cc-type"
  );
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: creditCards[0],
    }
  );
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD["cc-name"] + "test",
    "cc name changed"
  );
  is(
    creditCards[0]["cc-type"],
    "visa",
    "unknown cc-type removed and next autodetected to visa upon manual save"
  );
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_editInvalidCreditCardNumber() {
  await setStorage(TEST_ADDRESS_4);
  let addresses = await getAddresses();
  let billingAddress = addresses[0];

  const INVALID_CREDIT_CARD_NUMBER = "123456789";
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_2, {
    billingAddressGUID: billingAddress.guid,
    guid: "invalid-number",
    version: 2,
    "cc-number": INVALID_CREDIT_CARD_NUMBER,
  });

  // Directly use FormAutofillStorage so we can set
  // sourceSync: true, since saveCreditCard uses FormAutofillParent
  // which doesn't expose this option.
  let { formAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  );
  await formAutofillStorage.initialize();
  // Use `sourceSync: true` to bypass field normalization which will
  // fail due to the invalid credit card number.
  await formAutofillStorage.creditCards.add(TEST_CREDIT_CARD, {
    sourceSync: true,
  });

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  is(
    creditCards[0]["cc-number"],
    "*********",
    "invalid credit card number stored"
  );
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      is(
        win.document.querySelector("#cc-number").value,
        INVALID_CREDIT_CARD_NUMBER,
        "cc-number field should be showing invalid credit card number"
      );
      is(
        win.document.querySelector("#cc-number").checkValidity(),
        false,
        "cc-number is invalid"
      );
      win.document.querySelector("#cancel").click();
    },
    {
      record: creditCards[0],
      skipDecryption: true,
    }
  );
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(
    creditCards[0]["cc-number"],
    "*********",
    "invalid cc number still in record"
  );
  await removeCreditCards([creditCards[0].guid]);
  await removeAddresses([addresses[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
  addresses = await getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});

add_task(async function test_editCreditCardWithInvalidNumber() {
  const TEST_CREDIT_CARD = Object.assign({}, TEST_CREDIT_CARD_1);
  await setStorage(TEST_CREDIT_CARD);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      ok(
        win.document.documentElement
          .querySelector("title")
          .textContent.includes("Edit"),
        "Edit card dialog title is correct"
      );
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      is(
        win.document.querySelector("#cc-number").validity.customError,
        false,
        "cc-number field should not have a custom error"
      );
      EventUtils.synthesizeKey("4111111111111112", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      is(
        win.document.querySelector("#cc-number").validity.customError,
        true,
        "cc-number field should have a custom error"
      );
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      win.document.querySelector("#cancel").click();
    },
    {
      record: creditCards[0],
    }
  );
  ok(true, "Edit credit card dialog is closed");
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_noAutocompletePopupOnSystemTab() {
  await setStorage(TEST_CREDIT_CARD_1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PRIVACY_PREF_URL },
    async browser => {
      // Open credit card manage dialog
      await SpecialPowers.spawn(browser, [], async () => {
        let button = content.document.querySelector(
          "#creditCardAutofill button"
        );
        button.click();
      });
      let dialog = await waitForSubDialogLoad(
        content,
        MANAGE_CREDIT_CARDS_DIALOG_URL
      );

      // Open edit credit card dialog
      await SpecialPowers.spawn(dialog, [], async () => {
        let button = content.document.querySelector("#add");
        button.click();
      });
      dialog = await waitForSubDialogLoad(content, EDIT_CREDIT_CARD_DIALOG_URL);

      // Focus on credit card number field
      await SpecialPowers.spawn(dialog, [], async () => {
        let number = content.document.querySelector("#cc-number");
        number.focus();
      });

      // autocomplete popup should not appear
      await ensureNoAutocompletePopup(browser);
    }
  );

  await removeAllRecords();
});
