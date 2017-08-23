/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

add_task(async function test_cancelEditCreditCardDialog() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_CREDIT_CARD_DIALOG_URL);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit credit card dialog is closed");
        resolve();
      }, {once: true});
      win.document.querySelector("#cancel").click();
    }, {once: true});
  });
});

add_task(async function test_cancelEditCreditCardDialogWithESC() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_CREDIT_CARD_DIALOG_URL);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit credit card dialog is closed with ESC key");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    }, {once: true});
  });
});

add_task(async function test_saveCreditCard() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_CREDIT_CARD_DIALOG_URL);
    win.addEventListener("load", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit credit card dialog is closed");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-number"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-name"], {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("0" + TEST_CREDIT_CARD_1["cc-exp-month"].toString(), {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-exp-year"].toString(), {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      info("saving credit card");
      EventUtils.synthesizeKey("VK_RETURN", {}, win);
    }, {once: true});
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  for (let [fieldName, fieldValue] of Object.entries(TEST_CREDIT_CARD_1)) {
    if (fieldName === "cc-number") {
      fieldValue = "*".repeat(fieldValue.length - 4) + fieldValue.substr(-4);
    }
    is(creditCards[0][fieldName], fieldValue, "check " + fieldName);
  }
  ok(creditCards[0]["cc-number-encrypted"], "cc-number-encrypted exists");
});

add_task(async function test_editCreditCard() {
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_CREDIT_CARD_DIALOG_URL, null, null, creditCards[0]);
    win.addEventListener("FormReady", () => {
      win.addEventListener("unload", () => {
        ok(true, "Edit credit card dialog is closed");
        resolve();
      }, {once: true});
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    }, {once: true});
  });
  creditCards = await getCreditCards();

  is(creditCards.length, 1, "only one credit card is in storage");
  is(creditCards[0]["cc-name"], TEST_CREDIT_CARD_1["cc-name"] + "test", "cc name changed");
  await removeCreditCards([creditCards[0].guid]);

  creditCards = await getCreditCards();
  is(creditCards.length, 0, "Credit card storage is empty");
});

add_task(async function test_addInvalidCreditCard() {
  await new Promise(resolve => {
    let win = window.openDialog(EDIT_CREDIT_CARD_DIALOG_URL);
    win.addEventListener("FormReady", () => {
      const unloadHandler = () => ok(false, "Edit credit card dialog shouldn't be closed");
      win.addEventListener("unload", unloadHandler);

      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();

      is(win.document.querySelector("form").checkValidity(), false, "cc-number is invalid");
      SimpleTest.requestFlakyTimeout("Ensure the window remains open after save attempt");
      setTimeout(() => {
        win.removeEventListener("unload", unloadHandler);
        win.close();
        resolve();
      }, 500);
    }, {once: true});
  });
  let creditCards = await getCreditCards();

  is(creditCards.length, 0, "Credit card storage is empty");
});
