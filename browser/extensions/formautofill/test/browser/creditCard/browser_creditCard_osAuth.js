"use strict";

const PAGE_PREFS = "about:preferences";
const PAGE_PRIVACY = PAGE_PREFS + "#privacy";
const SELECTORS = {
  savedCreditCardsBtn: "#creditCardAutofill button",
  reauthCheckbox: "#creditCardReauthenticate checkbox",
};

// On mac, this test times out in chaos mode
requestLongerTimeout(2);

add_setup(async function () {
  // Revert head.js change that mocks os auth
  sinon.restore();

  // Load in a few credit cards
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.reduceTimerPrecision", false]],
  });
  await setStorage(TEST_CREDIT_CARD_1, TEST_CREDIT_CARD_2);
});

add_task(async function test_os_auth_enabled_with_checkbox() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;

      await SpecialPowers.spawn(
        browser,
        [SELECTORS, AppConstants.NIGHTLY_BUILD],
        async (selectors, isNightly) => {
          is(
            content.document.querySelector(selectors.reauthCheckbox).checked,
            isNightly,
            "OSReauth for credit cards should be checked"
          );
        }
      );
      is(
        FormAutofillUtils.getOSAuthEnabled(
          FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
        ),
        AppConstants.NIGHTLY_BUILD,
        "OSAuth should be enabled."
      );
    }
  );
});

add_task(async function test_os_auth_disabled_with_checkbox() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  FormAutofillUtils.setOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
    false
  );

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;

      await SpecialPowers.spawn(browser, [SELECTORS], async selectors => {
        is(
          content.document.querySelector(selectors.reauthCheckbox).checked,
          false,
          "OSReauth for credit cards should be unchecked"
        );
      });
      is(
        FormAutofillUtils.getOSAuthEnabled(
          FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
        ),
        false,
        "OSAuth should be disabled"
      );
    }
  );
  FormAutofillUtils.setOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
    true
  );
});

add_task(async function test_OSAuth_enabled_with_random_value_in_pref() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  await SpecialPowers.pushPrefEnv({
    set: [
      [FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF, "poutine-gravy"],
    ],
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], async selectors => {
        let reauthCheckbox = content.document.querySelector(
          selectors.reauthCheckbox
        );
        is(
          reauthCheckbox.checked,
          true,
          "OSReauth for credit cards should be checked"
        );
      });
      is(
        FormAutofillUtils.getOSAuthEnabled(
          FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
        ),
        true,
        "OSAuth should be enabled since the pref does not decrypt to 'opt out'."
      );
    }
  );
});

add_task(async function test_osAuth_enabled_behaviour() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  await SpecialPowers.pushPrefEnv({
    set: [[FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF, ""]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;
      if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
        // The rest of the test uses Edit mode which causes an OS prompt in official builds.
        return;
      }
      let reauthObserved = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
      await SpecialPowers.spawn(browser, [SELECTORS], async selectors => {
        content.document.querySelector(selectors.savedCreditCardsBtn).click();
      });
      let ccManageDialog = await waitForSubDialogLoad(
        content,
        MANAGE_CREDIT_CARDS_DIALOG_URL
      );
      await SpecialPowers.spawn(ccManageDialog, [], async () => {
        let selRecords = content.document.getElementById("credit-cards");
        await EventUtils.synthesizeMouseAtCenter(
          selRecords.children[0],
          [],
          content
        );
        content.document.querySelector("#edit").click();
      });
      await reauthObserved; // If the OS does not popup, this will cause a timeout in the test.
      await waitForSubDialogLoad(content, EDIT_CREDIT_CARD_DIALOG_URL);
    }
  );
});

add_task(async function test_osAuth_disabled_behavior() {
  let finalPrefPaneLoaded = TestUtils.topicObserved("sync-pane-loaded");
  FormAutofillUtils.setOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
    false
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function (browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(
        browser,
        [SELECTORS.savedCreditCardsBtn, SELECTORS.reauthCheckbox],
        async (saveButton, reauthCheckbox) => {
          is(
            content.document.querySelector(reauthCheckbox).checked,
            false,
            "OSReauth for credit cards should NOT be checked"
          );
          content.document.querySelector(saveButton).click();
        }
      );
      let ccManageDialog = await waitForSubDialogLoad(
        content,
        MANAGE_CREDIT_CARDS_DIALOG_URL
      );
      await SpecialPowers.spawn(ccManageDialog, [], async () => {
        let selRecords = content.document.getElementById("credit-cards");
        await EventUtils.synthesizeMouseAtCenter(
          selRecords.children[0],
          [],
          content
        );
        content.document.getElementById("edit").click();
      });
      info("The OS Auth dialog should NOT show up");
      // If OSAuth prompt shows up, the next line would cause a timeout since the edit dialog would not show up.
      await waitForSubDialogLoad(content, EDIT_CREDIT_CARD_DIALOG_URL);
    }
  );
  FormAutofillUtils.setOSAuthEnabled(
    FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF,
    true
  );
});
