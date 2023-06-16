"use strict";

add_task(async function test_update_autofill_name_field() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let osKeyStoreLoginShown =
        OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      let onPopupShown = waitForPopupShown();

      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await waitForAutofill(browser, "#cc-name", "John Doe");

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(creditCards[0]["cc-name"], "User 1", "cc-name field is updated");
  is(
    creditCards[0]["cc-number"],
    "************1111",
    "Verify the card number field"
  );
  await removeAllRecords();
});

add_task(async function test_update_autofill_exp_date_field() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let osKeyStoreLoginShown =
        OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      let onPopupShown = waitForPopupShown();
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await waitForAutofill(browser, "#cc-name", "John Doe");

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-exp-year": "2019",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(creditCards[0]["cc-exp-year"], 2019, "cc-exp-year field is updated");
  is(
    creditCards[0]["cc-number"],
    "************1111",
    "Verify the card number field"
  );
  await removeAllRecords();
});

add_task(async function test_submit_unnormailzed_field() {
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "John Doe",
          "#cc-number": "4111111111111111",
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear().toString().substr(2, 2),
          "#cc-type": "visa",
        },
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(
    creditCards[0]["cc-exp-year"],
    new Date().getFullYear(),
    "Verify the expiry year field"
  );
  await removeAllRecords();
});

add_task(async function test_submit_invalid_network_field() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": "5038146897157463",
          "#cc-exp-month": "12",
          "#cc-exp-year": "2017",
          "#cc-type": "gringotts",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 1", "Verify the name field");
  is(
    creditCards[0]["cc-type"],
    undefined,
    "Invalid network/cc-type was not saved"
  );

  await removeAllRecords();
});

add_task(async function test_submit_combined_expiry_field() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_COMBINED_EXPIRY_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "John Doe",
          "#cc-number": "374542158116607",
          "#cc-exp": "05/28",
        },
      });
      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "Card should be added");
  is(creditCards[0]["cc-exp"], "2028-05", "Verify cc-exp field");
  is(creditCards[0]["cc-exp-month"], 5, "Verify cc-exp-month field");
  is(creditCards[0]["cc-exp-year"], 2028, "Verify cc-exp-year field");
  await removeAllRecords();
});
