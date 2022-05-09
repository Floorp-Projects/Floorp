"use strict";
/*
  We use arbitrary timeouts here to ensure that the input event queue is cleared
  before we assert any information about the UI, otherwise there's a reasonable chance
  that the UI won't be ready and we will get an invalid test result.
*/
add_task(async function test_submit_creditCard_cancel_saving() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
});

add_task(async function test_submit_creditCard_saved() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });

  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": "5038146897157463",
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_untouched_creditCard_form() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await waitForAutofill(browser, "#cc-name", "John Doe");

      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(creditCards[0].timesUsed, 1, "timesUsed field set to 1");
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    3,
    "User has used autofill"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_untouched_creditCard_form_iframe() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  // This test triggers two form submission events so cc 'timesUsed' count is 2.
  // The first submission is triggered by standard form submission, and the
  // second is triggered by page hiding.
  const EXPECTED_ON_USED_COUNT = 2;
  let notifyUsedCounter = EXPECTED_ON_USED_COUNT;
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => {
      if (data == "notifyUsed") {
        notifyUsedCounter--;
      }
      return notifyUsedCounter == 0;
    }
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
      let iframeBC = browser.browsingContext.children[0];
      await openPopupOnSubframe(browser, iframeBC, "form #cc-name");

      EventUtils.synthesizeKey("VK_DOWN", {});
      EventUtils.synthesizeKey("VK_RETURN", {});
      await osKeyStoreLoginShown;
      await waitForAutofill(iframeBC, "#cc-name", "John Doe");

      await SpecialPowers.spawn(iframeBC, [], async function() {
        let form = content.document.getElementById("form");
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(
    creditCards[0].timesUsed,
    EXPECTED_ON_USED_COUNT,
    "timesUsed field set to 2"
  );
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    3,
    "User has used autofill"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_iframe_unload_save_card() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      let iframeBC = browser.browsingContext.children[0];
      await focusUpdateSubmitForm(
        iframeBC,
        {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-name": "User 1",
            "#cc-number": "4556194630960970",
            "#cc-exp-month": "10",
            "#cc-exp-year": "2024",
            "#cc-type": "visa",
          },
        },
        false
      );

      info("Removing iframe without submitting");
      await SpecialPowers.spawn(browser, [], async function() {
        let frame = content.document.querySelector("iframe");
        frame.remove();
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 1", "Verify the name field");
  is(creditCards[0]["cc-type"], "visa", "Verify the cc-type field");
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_changed_subset_creditCard_form() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_duplicate_creditCard_form() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "John Doe",
          "#cc-number": "4111111111111111",
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
          "#cc-type": "visa",
        },
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(
    creditCards[0]["cc-name"],
    TEST_CREDIT_CARD_1["cc-name"],
    "Verify the name field"
  );
  is(creditCards[0].timesUsed, 1, "timesUsed field set to 1");
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    1,
    "User neither sees the doorhanger nor uses autofill but somehow has a record in the storage"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_unnormailzed_creditCard_form() {
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "John Doe",
          "#cc-number": "4111111111111111",
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date()
            .getFullYear()
            .toString()
            .substr(2, 2),
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

add_task(async function test_submit_creditCard_never_save() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
});

add_task(async function test_submit_creditCard_with_sync_account() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SYNC_USERNAME_PREF, "foo@bar.com"],
      [SYNC_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 2",
          "#cc-number": "6387060366272981",
        },
      });

      await onPopupShown;
      let cb = getDoorhangerCheckbox();
      ok(!cb.hidden, "Sync checkbox should be visible");
      is(
        SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF),
        false,
        "creditCards sync should be disabled by default"
      );

      // Verify if the checkbox and button state is changed.
      let secondaryButton = getDoorhangerButton(SECONDARY_BUTTON);
      let menuButton = getDoorhangerButton(MENU_BUTTON);
      is(
        cb.checked,
        false,
        "Checkbox state should match creditCards sync state"
      );
      is(
        secondaryButton.disabled,
        false,
        "Not saving button should be enabled"
      );
      is(
        menuButton.disabled,
        false,
        "Never saving menu button should be enabled"
      );
      // Click the checkbox to enable credit card sync.
      cb.click();
      is(
        SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF),
        true,
        "creditCards sync should be enabled after checked"
      );
      is(
        secondaryButton.disabled,
        true,
        "Not saving button should be disabled"
      );
      is(
        menuButton.disabled,
        true,
        "Never saving menu button should be disabled"
      );
      // Click the checkbox again to disable credit card sync.
      cb.click();
      is(
        SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF),
        false,
        "creditCards sync should be disabled after unchecked"
      );
      is(
        secondaryButton.disabled,
        false,
        "Not saving button should be enabled again"
      );
      is(
        menuButton.disabled,
        false,
        "Never saving menu button should be enabled again"
      );
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
});

add_task(async function test_submit_creditCard_with_synced_already() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SYNC_CREDITCARDS_PREF, true],
      [SYNC_USERNAME_PREF, "foo@bar.com"],
      [SYNC_CREDITCARDS_AVAILABLE_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 2",
          "#cc-number": "6387060366272981",
        },
      });

      await onPopupShown;
      let cb = getDoorhangerCheckbox();
      ok(cb.hidden, "Sync checkbox should be hidden");
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
});

add_task(async function test_submit_manual_mergeable_creditCard_form() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_3);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 3",
          "#cc-number": "5103059495477870",
          "#cc-exp-month": "1",
          "#cc-exp-year": "2000",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 3", "Verify the name field");
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_update_autofill_form_name() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    3,
    "User has used autofill"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_update_autofill_form_exp_date() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    3,
    "User has used autofill"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_create_new_autofill_form() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
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
      await osKeyStoreLoginShown;
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
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    3,
    "User has used autofill"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_update_duplicate_autofill_form() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await setStorage(
    { "cc-number": "6387060366272981" },
    { "cc-number": "5038146897157463" }
  );
  let creditCards = await getCreditCards();
  is(creditCards.length, 2, "2 credit card in storage");
  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(
        true
      );
      await openPopupOn(browser, "form #cc-number");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#cc-number", "6387060366272981");

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          // Change number to the second credit card number
          "#cc-number": "5038146897157463",
        },
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      await osKeyStoreLoginShown;
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 2, "Still 2 credit card");
  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    1,
    "User neither sees the doorhanger nor uses autofill but somehow has a record in the storage"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_creditCard_with_invalid_network() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
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

  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();
});

add_task(async function test_submit_form_with_combined_expiry_field() {
  let onChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_COMBINED_EXPIRY_URL },
    async function(browser) {
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

/*
  The next four tests look very similar because if we try to do multiple
  credit card operations in one test, there's a good chance the test will timeout
  and produce an invalid result.
  We mitigate this issue by having each test only deal with one credit card in storage
  and one credit card operation.
*/
add_task(async function test_submit_third_party_creditCard_logo() {
  const amexCard = {
    "cc-number": "374542158116607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": amexCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/third-party/cc-logo-amex",
        "CC logo should be amex"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
  await removeAllRecords();
});

add_task(async function test_update_third_party_creditCard_logo() {
  const amexCard = {
    "cc-number": "374542158116607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };

  await setStorage(amexCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": amexCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/third-party/cc-logo-amex`,
        `CC Logo should be amex`
      );
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});

add_task(async function test_submit_generic_creditCard_logo() {
  const genericCard = {
    "cc-number": "937899583135",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "User 1",
          "#cc-number": genericCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/icon-credit-card-generic",
        "CC logo should be generic"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
  await removeAllRecords();
});

add_task(async function test_update_generic_creditCard_logo() {
  const genericCard = {
    "cc-number": "937899583135",
    "cc-name": "John Doe",
  };

  await setStorage(genericCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": genericCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/icon-credit-card-generic`,
        `CC Logo should be generic`
      );
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});

add_task(async function test_save_panel_spaces_in_cc_number_logo() {
  const amexCard = {
    "cc-number": "37 4542 158116 607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-number": amexCard["cc-number"],
        },
      });

      await onPopupShown;
      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];

      is(
        creditCardLogoWithoutExtension,
        "chrome://formautofill/content/third-party/cc-logo-amex",
        "CC logo should be amex"
      );

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
});

add_task(async function test_update_panel_with_spaces_in_cc_number_logo() {
  const amexCard = {
    "cc-number": "374 54215 8116607",
    "cc-type": "amex",
    "cc-name": "John Doe",
  };

  await setStorage(amexCard);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onChanged = waitForStorageChangedEvents("update", "notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-name",
        newValues: {
          "#cc-name": "Mark Smith",
          "#cc-number": amexCard["cc-number"],
          "#cc-exp-month": "4",
          "#cc-exp-year": new Date().getFullYear(),
        },
      });

      await onPopupShown;

      let doorhanger = getNotification();
      let creditCardLogo = doorhanger.querySelector(".desc-message-box image");
      let creditCardLogoWithoutExtension = creditCardLogo.src.split(".", 1)[0];
      is(
        creditCardLogoWithoutExtension,
        `chrome://formautofill/content/third-party/cc-logo-amex`,
        `CC Logo should be amex`
      );
      // We are not testing whether the cc-number is saved correctly,
      // we only care that the logo in the update panel shows correctly.
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onChanged;
  await removeAllRecords();
});
