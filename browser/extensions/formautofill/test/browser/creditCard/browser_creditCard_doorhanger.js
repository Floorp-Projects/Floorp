/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_submit_creditCard_cancel_saving() {
  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        let number = form.querySelector("#cc-number");
        number.setUserInput("5038146897157463");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      ok(
        !SpecialPowers.Services.prefs.prefHasUserValue(SYNC_USERNAME_PREF),
        "Sync account should not exist by default"
      );
      let cb = getDoorhangerCheckbox();
      ok(cb.hidden, "Sync checkbox should be hidden");
      await promiseShown;
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
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      let onChanged = TestUtils.topicObserved("formautofill-storage-changed");
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        form.querySelector("#cc-number").setUserInput("5038146897157463");
        form.querySelector("#cc-exp-month").setUserInput("12");
        form.querySelector("#cc-exp-year").setUserInput("2017");
        form.querySelector("#cc-type").value = "mastercard";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await onChanged;
    }
  );

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function(browser) {
      let iframeBC = browser.browsingContext.children[0];
      await openPopupForSubframe(browser, iframeBC, "form #cc-name");
      EventUtils.synthesizeKey("VK_DOWN", {});
      EventUtils.synthesizeKey("VK_RETURN", {});
      await osKeyStoreLoginShown;
      await SpecialPowers.spawn(iframeBC, [], async function() {
        let form = content.document.getElementById("form");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  creditCards = await getCreditCards();
  is(creditCards.length, 1, "Still 1 credit card");
  is(creditCards[0].timesUsed, 2, "timesUsed field set to 2");
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
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_IFRAME_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      let iframeBC = browser.browsingContext.children[0];
      let onChanged = TestUtils.topicObserved("formautofill-storage-changed");
      await SpecialPowers.spawn(iframeBC, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        form.querySelector("#cc-number").setUserInput("4556194630960970");
        form.querySelector("#cc-exp-month").setUserInput("10");
        form.querySelector("#cc-exp-year").setUserInput("2024");
        form.querySelector("#cc-type").value = "visa";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
      });

      info("Removing iframe without submitting");
      await SpecialPowers.spawn(browser, [], async function() {
        let frame = content.document.querySelector("iframe");
        frame.remove();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await onChanged;
    }
  );

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");

        name.focus();
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        name.setUserInput("Mark Smith");

        form.querySelector("#cc-number").setUserInput("4111111111111111");
        form.querySelector("#cc-exp-month").setUserInput("4");
        form
          .querySelector("#cc-exp-year")
          .setUserInput(new Date().getFullYear());
        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onUsed;

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();

        name.setUserInput("John Doe");
        form.querySelector("#cc-number").setUserInput("4111111111111111");
        form.querySelector("#cc-exp-month").setUserInput("4");
        form
          .querySelector("#cc-exp-year")
          .setUserInput(new Date().getFullYear());
        form.querySelector("#cc-type").value = "visa";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();

        name.setUserInput("John Doe");
        form.querySelector("#cc-number").setUserInput("4111111111111111");
        form.querySelector("#cc-exp-month").setUserInput("4");
        // Set unnormalized year
        form.querySelector("#cc-exp-year").setUserInput(
          new Date()
            .getFullYear()
            .toString()
            .substr(2, 2)
        );
        form.querySelector("#cc-type").value = "visa";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
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
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        name.setUserInput("User 0");

        let number = form.querySelector("#cc-number");
        number.setUserInput("6387060366272981");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
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
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("6387060366272981");

        // Wait 500ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 500));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
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
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("6387060366272981");

        // Wait 500ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 500));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
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
  await saveCreditCard(TEST_CREDIT_CARD_3);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();

        name.setUserInput("User 3");
        form.querySelector("#cc-number").setUserInput("5103059495477870");
        form.querySelector("#cc-exp-month").setUserInput("1");
        form.querySelector("#cc-exp-year").setUserInput("2000");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });
      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onUsed;

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await SpecialPowers.spawn(browser, [], async function() {
        await ContentTaskUtils.waitForCondition(() => {
          let form = content.document.getElementById("form");
          let name = form.querySelector("#cc-name");
          return name.value == "John Doe";
        }, "Credit card detail never fills");
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.setUserInput("User 1");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });
      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
  await onUsed;

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await SpecialPowers.spawn(browser, [], async function() {
        await ContentTaskUtils.waitForCondition(() => {
          let form = content.document.getElementById("form");
          let name = form.querySelector("#cc-name");
          return name.value == "John Doe";
        }, "Credit card detail never fills");
        let form = content.document.getElementById("form");
        let year = form.querySelector("#cc-exp-year");
        year.setUserInput("2019");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });
      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await osKeyStoreLoginShown;
    }
  );
  await onUsed;

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
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      let onChanged = TestUtils.topicObserved("formautofill-storage-changed");
      await openPopupOn(browser, "form #cc-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await SpecialPowers.spawn(browser, [], async function() {
        await ContentTaskUtils.waitForCondition(() => {
          let form = content.document.getElementById("form");
          let name = form.querySelector("#cc-name");
          return name.value == "John Doe";
        }, "Credit card detail never fills");
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.setUserInput("User 1");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
      await osKeyStoreLoginShown;
      await onChanged;
    }
  );

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
  await saveCreditCard({
    "cc-number": "6387060366272981",
  });
  await saveCreditCard({
    "cc-number": "5038146897157463",
  });
  let creditCards = await getCreditCards();
  is(creditCards.length, 2, "2 credit card in storage");
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await openPopupOn(browser, "form #cc-number");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await SpecialPowers.spawn(browser, [], async function() {
        await ContentTaskUtils.waitForCondition(() => {
          let form = content.document.getElementById("form");
          let number = form.querySelector("#cc-number");
          return number.value == "6387060366272981";
        }, "Should be the first credit card number");

        // Change number to the second credit card number
        let form = content.document.getElementById("form");
        let number = form.querySelector("#cc-number");
        number.setUserInput("5038146897157463");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
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
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshown"
      );
      let onChanged = TestUtils.topicObserved("formautofill-storage-changed");
      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        form.querySelector("#cc-number").setUserInput("5038146897157463");
        form.querySelector("#cc-exp-month").setUserInput("12");
        form.querySelector("#cc-exp-year").setUserInput("2017");
        form.querySelector("#cc-type").value = "gringotts";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await onChanged;
    }
  );

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
