"use strict";

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_manageCreditCardsInitialState() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: MANAGE_CREDIT_CARDS_DIALOG_URL },
    async function(browser) {
      await ContentTask.spawn(browser, TEST_SELECTORS, args => {
        let selRecords = content.document.querySelector(args.selRecords);
        let btnRemove = content.document.querySelector(args.btnRemove);
        let btnAdd = content.document.querySelector(args.btnAdd);
        let btnEdit = content.document.querySelector(args.btnEdit);

        is(selRecords.length, 0, "No credit card");
        is(btnRemove.disabled, true, "Remove button disabled");
        is(btnAdd.disabled, false, "Add button enabled");
        is(btnEdit.disabled, true, "Edit button disabled");
      });
    }
  );
});

add_task(async function test_cancelManageCreditCardsDialogWithESC() {
  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL);
  await waitForFocusAndFormReady(win);
  let unloadPromise = BrowserTestUtils.waitForEvent(win, "unload");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  await unloadPromise;
  ok(true, "Manage credit cards dialog is closed with ESC key");
});

add_task(async function test_removingSingleAndMultipleCreditCards() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.reduceTimerPrecision", false]],
  });
  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);

  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selRecords.length, 3, "Three credit cards");
  is(selRecords[0].text, "**** 7870", "Masked credit card 3");
  is(
    selRecords[1].text,
    "**** 1045, Timothy Berners-Lee",
    "Masked credit card 2"
  );
  is(selRecords[2].text, "**** 1111, John Doe", "Masked credit card 1");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 2, "Two credit cards left");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(
    selRecords.children[1],
    { shiftKey: true },
    win
  );
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "All credit cards are removed");

  win.close();
});

add_task(async function test_removingCreditCardsViaKeyboardDelete() {
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  is(selRecords.length, 1, "One credit card");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeKey("VK_DELETE", {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "No credit cards left");

  win.close();
});

add_task(async function test_creditCardsDialogWatchesStorageChanges() {
  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  await saveCreditCard(TEST_CREDIT_CARD_1);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords.length, 1, "One credit card is shown");

  await removeCreditCards([selRecords.options[0].value]);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords.length, 0, "Credit card is removed");
  win.close();
});

add_task(async function test_showCreditCardIcons() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.reduceTimerPrecision", false]],
  });
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let unknownCard = Object.assign({}, TEST_CREDIT_CARD_3, {
    "cc-type": "gringotts",
  });
  await saveCreditCard(unknownCard);

  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  is(
    selRecords.classList.contains("branded"),
    AppConstants.MOZILLA_OFFICIAL,
    "record picker has 'branded' class in an MOZILLA_OFFICIAL build"
  );

  let option0 = selRecords.options[0];
  let icon0Url = win.getComputedStyle(option0, "::before").backgroundImage;
  let option1 = selRecords.options[1];
  let icon1Url = win.getComputedStyle(option1, "::before").backgroundImage;

  is(
    option0.getAttribute("cc-type"),
    "gringotts",
    "Option has the expected cc-type"
  );
  is(
    option1.getAttribute("cc-type"),
    "visa",
    "Option has the expected cc-type"
  );

  if (AppConstants.MOZILLA_OFFICIAL) {
    ok(
      icon0Url.includes("icon-credit-card-generic.svg"),
      "unknown network option ::before element has the generic icon as backgroundImage: " +
        icon0Url
    );
    ok(
      icon1Url.includes("cc-logo-visa.svg"),
      "visa option ::before element has the visa icon as backgroundImage " +
        icon1Url
    );
  }

  await removeCreditCards([option0.value, option1.value]);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords.length, 0, "Credit card is removed");
  win.close();
});

add_task(async function test_hasEditLoginPrompt() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await saveCreditCard(TEST_CREDIT_CARD_1);

  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnAdd = win.document.querySelector(TEST_SELECTORS.btnAdd);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(); // cancel
  EventUtils.synthesizeMouseAtCenter(btnEdit, {}, win);
  await osKeyStoreLoginShown;
  await new Promise(resolve => waitForFocus(resolve, win));
  await new Promise(resolve => executeSoon(resolve));

  // Login is not required for removing credit cards.
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "Credit card is removed");

  // gSubDialog.open should be called when trying to add a credit card,
  // no OS login dialog is required.
  window.gSubDialog = {
    open: url =>
      is(url, EDIT_CREDIT_CARD_DIALOG_URL, "Edit credit card dialog is called"),
  };
  EventUtils.synthesizeMouseAtCenter(btnAdd, {}, win);
  delete window.gSubDialog;

  win.close();
});
