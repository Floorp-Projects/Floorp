/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_submit_creditCard_cancel_saving() {
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1111222233334444");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  await sleep(1000);
  let creditCards = await getCreditCards();
  is(creditCards.length, 0, "No credit card saved");
});

add_task(async function test_submit_creditCard_saved() {
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 1");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1111222233334444");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");
  is(creditCards[0]["cc-name"], "User 1", "Verify the name field");
});

add_task(async function test_submit_creditCard_never_save() {
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        await new Promise(resolve => setTimeout(resolve, 1000));
        name.setUserInput("User 0");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1234123412341234");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MENU_BUTTON, 0);
    }
  );

  await sleep(1000);
  let creditCards = await getCreditCards();
  let creditCardPref = SpecialPowers.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  is(creditCards.length, 1, "Still 1 credit card in storage");
  is(creditCardPref, false, "Credit card is disabled");
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
});

add_task(async function test_submit_creditCard_saved_with_mp_enabled() {
  LoginTestUtils.masterPassword.enable();
  // Login with the masterPassword in LoginTestUtils.
  let masterPasswordDialogShown = waitForMasterPasswordDialog(true);
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        await new Promise(resolve => setTimeout(resolve, 1000));
        name.setUserInput("User 0");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1234123412341234");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await masterPasswordDialogShown;
      await TestUtils.topicObserved("formautofill-storage-changed");
    }
  );

  let creditCards = await getCreditCards();
  is(creditCards.length, 2, "2 credit cards in storage");
  is(creditCards[1]["cc-name"], "User 0", "Verify the name field");
  is(creditCards[1]["cc-number"], "************1234", "Verify the card number field");
  LoginTestUtils.masterPassword.disable();
});

add_task(async function test_submit_creditCard_saved_with_mp_enabled_but_canceled() {
  LoginTestUtils.masterPassword.enable();
  let masterPasswordDialogShown = waitForMasterPasswordDialog();
  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        await new Promise(resolve => setTimeout(resolve, 1000));
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("5678567856785678");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      await clickDoorhangerButton(MAIN_BUTTON);
      await masterPasswordDialogShown;
    }
  );

  await sleep(1000);
  let creditCards = await getCreditCards();
  is(creditCards.length, 2, "Still 2 credit cards in storage");
  LoginTestUtils.masterPassword.disable();
});

add_task(async function test_submit_creditCard_unavailable_with_sync_account() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      [SYNC_USERNAME_PREF, "foo@bar.com"],
    ],
  });

  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      is(SpecialPowers.getBoolPref(SYNC_CREDITCARDS_AVAILABLE_PREF), false,
         "creditCards sync should be unavailable by default");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1234123412341234");

        // Wait 500ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 500));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      let cb = getDoorhangerCheckbox();
      ok(cb.hidden, "Sync checkbox should be hidden");

      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );
});

add_task(async function test_submit_creditCard_with_sync_account() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      [SYNC_USERNAME_PREF, "foo@bar.com"],
      [SYNC_CREDITCARDS_AVAILABLE_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1234123412341234");

        // Wait 500ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 500));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      let cb = getDoorhangerCheckbox();
      ok(!cb.hidden, "Sync checkbox should be visible");
      is(SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF), false,
         "creditCards sync should be disabled by default");

      // Verify if the checkbox and button state is changed.
      let secondaryButton = getDoorhangerButton(SECONDARY_BUTTON);
      let menuButton = getDoorhangerButton(MENU_BUTTON);
      is(cb.checked, false, "Checkbox state should match creditCards sync state");
      is(secondaryButton.disabled, false, "Not saving button should be enabled");
      is(menuButton.disabled, false, "Never saving menu button should be enabled");
      // Click the checkbox to enable credit card sync.
      cb.click();
      is(SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF), true,
         "creditCards sync should be enabled after checked");
      is(secondaryButton.disabled, true, "Not saving button should be disabled");
      is(menuButton.disabled, true, "Never saving menu button should be disabled");
      // Click the checkbox again to disable credit card sync.
      cb.click();
      is(SpecialPowers.getBoolPref(SYNC_CREDITCARDS_PREF), false,
         "creditCards sync should be disabled after unchecked");
      is(secondaryButton.disabled, false, "Not saving button should be enabled again");
      is(menuButton.disabled, false, "Never saving menu button should be enabled again");
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
});

add_task(async function test_submit_creditCard_with_synced_already() {
  await SpecialPowers.pushPrefEnv({
    "set": [
      [SYNC_CREDITCARDS_PREF, true],
      [SYNC_USERNAME_PREF, "foo@bar.com"],
      [SYNC_CREDITCARDS_AVAILABLE_PREF, true],
    ],
  });

  await BrowserTestUtils.withNewTab({gBrowser, url: CREDITCARD_FORM_URL},
    async function(browser) {
      let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                       "popupshown");
      await ContentTask.spawn(browser, null, async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");
        name.focus();
        name.setUserInput("User 2");

        let number = form.querySelector("#cc-number");
        number.setUserInput("1234123412341234");

        // Wait 500ms before submission to make sure the input value applied
        await new Promise(resolve => setTimeout(resolve, 500));
        form.querySelector("input[type=submit]").click();
      });

      await promiseShown;
      let cb = getDoorhangerCheckbox();
      ok(cb.hidden, "Sync checkbox should be hidden");
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );
});
