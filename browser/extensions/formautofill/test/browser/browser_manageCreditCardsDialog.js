"use strict";

Cu.import("resource://testing-common/LoginTestUtils.jsm", this);

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnShowCreditCards: "#show-credit-cards",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_manageCreditCardsInitialState() {
  await BrowserTestUtils.withNewTab({gBrowser, url: MANAGE_CREDIT_CARDS_DIALOG_URL}, async function(browser) {
    await ContentTask.spawn(browser, TEST_SELECTORS, (args) => {
      let selRecords = content.document.querySelector(args.selRecords);
      let btnRemove = content.document.querySelector(args.btnRemove);
      let btnShowCreditCards = content.document.querySelector(args.btnShowCreditCards);
      let btnAdd = content.document.querySelector(args.btnAdd);
      let btnEdit = content.document.querySelector(args.btnEdit);

      is(selRecords.length, 0, "No credit card");
      is(btnRemove.disabled, true, "Remove button disabled");
      is(btnShowCreditCards.disabled, false, "Show Credit Cards button disabled");
      is(btnAdd.disabled, false, "Add button enabled");
      is(btnEdit.disabled, true, "Edit button disabled");
    });
  });
});

add_task(async function test_cancelManageCreditCardsDialogWithESC() {
  await new Promise(resolve => {
    let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL);
    win.addEventListener("FormReady", () => {
      win.addEventListener("unload", () => {
        ok(true, "Manage credit cards dialog is closed with ESC key");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    }, {once: true});
  });
});

add_task(async function test_removingSingleAndMultipleCreditCards() {
  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await BrowserTestUtils.waitForEvent(win, "FormReady");

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);

  is(selRecords.length, 3, "Three credit cards");
  is(selRecords[0].text, "**** 6666", "Masked credit card 3");
  is(selRecords[1].text, "**** 4444, Timothy Berners-Lee", "Masked credit card 2");
  is(selRecords[2].text, "**** 5678, John Doe", "Masked credit card 1");

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

add_task(async function test_creditCardsDialogWatchesStorageChanges() {
  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await BrowserTestUtils.waitForEvent(win, "FormReady");

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_3);

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await BrowserTestUtils.waitForEvent(win, "FormReady");

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnShowCreditCards = win.document.querySelector(TEST_SELECTORS.btnShowCreditCards);

  EventUtils.synthesizeMouseAtCenter(btnShowCreditCards, {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "OptionsDecrypted");

  is(selRecords[0].text, "9999888877776666", "Decrypted credit card 3");
  is(selRecords[1].text, "1111222233334444, Timothy Berners-Lee", "Decrypted credit card 2");
  is(selRecords[2].text, "1234567812345678, John Doe", "Decrypted credit card 1");

  await removeCreditCards([selRecords.options[2].value]);
  await removeCreditCards([selRecords.options[1].value]);
  await removeCreditCards([selRecords.options[0].value]);
  win.close();
});

add_task(async function test_hasMasterPassword() {
  await saveCreditCard(TEST_CREDIT_CARD_1);
  LoginTestUtils.masterPassword.enable();

  let win = window.openDialog(MANAGE_CREDIT_CARDS_DIALOG_URL, null, DIALOG_SIZE);
  await BrowserTestUtils.waitForEvent(win, "FormReady");

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);
  let btnRemove = win.document.querySelector(TEST_SELECTORS.btnRemove);
  let btnShowCreditCards = win.document.querySelector(TEST_SELECTORS.btnShowCreditCards);
  let btnAdd = win.document.querySelector(TEST_SELECTORS.btnAdd);
  let btnEdit = win.document.querySelector(TEST_SELECTORS.btnEdit);
  let masterPasswordDialogShown = waitForMasterPasswordDialog();

  is(btnShowCreditCards.hidden, true, "Show credit cards button is hidden");

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
