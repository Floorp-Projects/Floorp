"use strict";

add_task(async function test_save_doorhanger_click_save() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": "5577000055770004",
          "#cc-exp-month": "12",
          "#cc-exp-year": "2017",
          "#cc-type": "mastercard",
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
  is(creditCards[0]["cc-type"], "mastercard", "Verify the cc-type field");
  await removeAllRecords();
});

add_task(async function test_save_doorhanger_click_never_save() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 0",
          "#cc-number": "6387060366272981",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MENU_BUTTON, 0);
    }
  );

  await sleep(1000);
  let creditCards = await getCreditCards();
  let creditCardPref = SpecialPowers.getBoolPref(
    ENABLED_AUTOFILL_CREDITCARDS_PREF
  );
  is(creditCards.length, 0, "No credit card in storage");
  is(creditCardPref, false, "Credit card is disabled");
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
});

add_task(async function test_save_doorhanger_click_cancel_save() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": "5038146897157463",
        },
      });

      ok(
        !SpecialPowers.Services.prefs.prefHasUserValue(SYNC_USERNAME_PREF),
        "Sync account should not exist by default"
      );
      await onPopupShown;
      let cb = getDoorhangerCheckbox();
      ok(cb.hidden, "Sync checkbox should be hidden");
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  await sleep(1000);
  let creditCards = await getCreditCards();
  is(creditCards.length, 0, "No credit card saved");
});

add_task(async function test_update_doorhanger_click_update() {
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": "4111111111111111",
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(creditCards[0]["cc-name"], "Mark Smith", "name field got updated");
  await removeAllRecords();
});

add_task(async function test_update_doorhanger_click_save() {
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
  let osKeyStoreLoginShown = null;
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      if (OSKeyStore.canReauth()) {
        osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      }
      let onPopupShown = waitForPopupShown();
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#cc-name", "John Doe");

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
      if (osKeyStoreLoginShown) {
        await osKeyStoreLoginShown;
        ok(osKeyStoreLoginShown, "OS re-auth promise Complete");
      }
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 2, "2 credit cards in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD_1["cc-name"],
    "Original record's cc-name field is unchanged"
  );
  is(creditCards[1]["cc-name"], "User 1", "cc-name field in the new record");
  await removeAllRecords();
});
