"use strict";

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnShowHideCreditCards: "#show-hide-credit-cards",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_manageCreditCardsInitialState() {
  await BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_CREDIT_CARDS_DIALOG_URL}, async function(browser) {
    await ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      let selRecords = content.document.querySelector(args.selRecords);
      let btnRemove = content.document.querySelector(args.btnRemove);
      let btnShowHideCreditCards = content.document.querySelector(args.btnShowHideCreditCards);
      let btnAdd = content.document.querySelector(args.btnAdd);
      let btnEdit = content.document.querySelector(args.btnEdit);

      is(selRecords.length, 0, "No credit card");
      is(btnRemove.disabled, true, "Remove button disabled");
      is(btnShowHideCreditCards.disabled, true, "Show Credit Cards button disabled");
      is(btnAdd.disabled, false, "Add button enabled");
      is(btnEdit.disabled, true, "Edit button disabled");
    });
  });
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
  await SpecialPowers.pushPrefEnv({"set": [["privacy.reduceTimerPrecision", false]]});
  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selRecords.length, 3, "Three credit cards");
  is(selRecords[0].text, "**** 7870", "Masked credit card 3");
  is(selRecords[1].text, "**** 1045, Timothy Berners-Lee", "Masked credit card 2");
  is(selRecords[2].text, "**** 1111, John Doe", "Masked credit card 1");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  is(btnRemove.disabled, false, "Remove button enabled");
  is(btnEdit.disabled, false, "Edit button enabled");
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 2, "Two credit cards left");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(selRecords.children[1],
                                     {shiftKey: true}, win);
  is(btnEdit.disabled, true, "Edit button disabled when multi-select");

  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "All credit cards are removed");

  win.close();
});

add_task(async function test_removingCreditCardsViaKeyboardDelete() {
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
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
  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
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

add_task(async function test_showCreditCards() {
  await SpecialPowers.pushPrefEnv({"set": [["privacy.reduceTimerPrecision", false]]});
  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnShowHideCreditCards = win.document.querySelector(TEST_SELECTORS.btnShowHideCreditCards);

  is(btnShowHideCreditCards.disabled, false, "Show credit cards button enabled");
  is(btnShowHideCreditCards.textContent, "Show Credit Cards", "Label should be 'Show Credit Cards'");

  // Show credit card numbers
  EventUtils.synthesizeMouseAtCenter(btnShowHideCreditCards, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "LabelsUpdated");
  is(selRecords[0].text, "5103059495477870", "Decrypted credit card 3");
  is(selRecords[1].text, "4929001587121045, Timothy Berners-Lee", "Decrypted credit card 2");
  is(selRecords[2].text, "4111111111111111, John Doe", "Decrypted credit card 1");
  is(btnShowHideCreditCards.textContent, "Hide Credit Cards", "Label should be 'Hide Credit Cards'");

  // Hide credit card numbers
  EventUtils.synthesizeMouseAtCenter(btnShowHideCreditCards, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "LabelsUpdated");
  is(selRecords[0].text, "**** 7870", "Masked credit card 3");
  is(selRecords[1].text, "**** 1045, Timothy Berners-Lee", "Masked credit card 2");
  is(selRecords[2].text, "**** 1111, John Doe", "Masked credit card 1");
  is(btnShowHideCreditCards.textContent, "Show Credit Cards", "Label should be 'Show Credit Cards'");

  // Show credit card numbers again to test if they revert back to masked form when reloaded
  EventUtils.synthesizeMouseAtCenter(btnShowHideCreditCards, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "LabelsUpdated");
  // Ensure credit card numbers are shown again
  is(selRecords[0].text, "5103059495477870", "Decrypted credit card 3");
  // Remove a card to trigger reloading
  await removeCreditCards([selRecords.options[2].value]);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(selRecords[0].text, "**** 7870", "Masked credit card 3");
  is(selRecords[1].text, "**** 1045, Timothy Berners-Lee", "Masked credit card 2");

  // Remove the rest of the cards
  await removeCreditCards([selRecords.options[1].value]);
  await removeCreditCards([selRecords.options[0].value]);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsLoaded");
  is(btnShowHideCreditCards.disabled, true, "Show credit cards button is disabled when there is no card");

  win.close();
});

add_task(async function test_hasMasterPassword() {
  await saveCreditCard(TEST_CREDIT_CARD_1);
  LoginTestUtils.masterPassword.enable();

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnShowHideCreditCards = win.document.querySelector(TEST_SELECTORS.btnShowHideCreditCards);
  let btnAdd = win.document.querySelector(TEST_SELECTORS.btnAdd);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);
  let masterPasswordDialogShown = waitForMasterPasswordDialog();

  is(btnShowHideCreditCards.hidden, true, "Show credit cards button is hidden");

  // Master password dialog should show when trying to edit a credit card record.
  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeMouseAtCenter(btnEdit, {}, win);
  await masterPasswordDialogShown;

  // Master password is not required for removing credit cards.
  EventUtils.synthesizeMouseAtCenter(btnRemove, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "Credit card is removed");

  // gSubDialog.open should be called when trying to add a credit card,
  // no master password is required.
  window.gSubDialog = {
    open: url => is(url, EDIT_CREDIT_CARD_DIALOG_URL, "Edit credit card dialog is called"),
  };
  EventUtils.synthesizeMouseAtCenter(btnAdd, {}, win);
  delete window.gSubDialog;

  win.close();
});
