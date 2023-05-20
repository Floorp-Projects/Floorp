"use strict";

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
    async function (browser) {
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
    async function (browser) {
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
