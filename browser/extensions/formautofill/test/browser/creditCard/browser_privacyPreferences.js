"use strict";

const PAGE_PREFS = "about:preferences";
const PAGE_PRIVACY = PAGE_PREFS + "#privacy";
const SELECTORS = {
  group: "#formAutofillGroupBox",
  addressAutofillCheckbox: "#addressAutofill checkbox",
  creditCardAutofillCheckbox: "#creditCardAutofill checkbox",
  savedAddressesBtn: "#addressAutofill button",
  savedCreditCardsBtn: "#creditCardAutofill button",
  addressAutofillLearnMore: "#addressAutofillLearnMore",
  creditCardAutofillLearnMore: "#creditCardAutofillLearnMore",
  reauthCheckbox: "#creditCardReauthenticate checkbox",
};

// Visibility of form autofill group should be hidden when opening
// preferences page.
add_task(async function test_aboutPreferences() {
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PREFS },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        is(
          content.document.querySelector(selectors.group).hidden,
          true,
          "Form Autofill group should be hidden"
        );
      });
    }
  );
});

// Visibility of form autofill group should be visible when opening
// directly to privacy page. Checkbox is checked by default.
add_task(async function test_aboutPreferencesPrivacy() {
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        is(
          content.document.querySelector(selectors.group).hidden,
          false,
          "Form Autofill group should be visible"
        );
        is(
          content.document.querySelector(selectors.addressAutofillCheckbox)
            .checked,
          true,
          "Autofill addresses checkbox should be checked"
        );
        is(
          content.document.querySelector(selectors.creditCardAutofillCheckbox)
            .checked,
          true,
          "Autofill credit cards checkbox should be checked"
        );
        ok(
          content.document
            .querySelector(selectors.addressAutofillLearnMore)
            .href.includes("autofill-card-address"),
          "Autofill addresses learn more link should contain autofill-card-address"
        );
        ok(
          content.document
            .querySelector(selectors.creditCardAutofillLearnMore)
            .href.includes("credit-card-autofill"),
          "Autofill credit cards learn more link should contain credit-card-autofill"
        );
      });
    }
  );
});

add_task(async function test_openManageAutofillDialogs() {
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      const args = [
        SELECTORS,
        MANAGE_ADDRESSES_DIALOG_URL,
        MANAGE_CREDIT_CARDS_DIALOG_URL,
      ];
      await SpecialPowers.spawn(
        browser,
        [args],
        ([selectors, addrUrl, ccUrl]) => {
          function testManageDialogOpened(expectedUrl) {
            return {
              open: openUrl => is(openUrl, expectedUrl, "Manage dialog called"),
            };
          }

          let realgSubDialog = content.window.gSubDialog;
          content.window.gSubDialog = testManageDialogOpened(addrUrl);
          content.document.querySelector(selectors.savedAddressesBtn).click();
          content.window.gSubDialog = testManageDialogOpened(ccUrl);
          content.document.querySelector(selectors.savedCreditCardsBtn).click();
          content.window.gSubDialog = realgSubDialog;
        }
      );
    }
  );
});

add_task(async function test_autofillCheckboxes() {
  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_ADDRESSES_PREF, false]],
  });
  await SpecialPowers.pushPrefEnv({
    set: [[ENABLED_AUTOFILL_CREDITCARDS_PREF, false]],
  });
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  // Checkbox should be unchecked when form autofill addresses and credit cards are disabled.
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        is(
          content.document.querySelector(selectors.group).hidden,
          false,
          "Form Autofill group should be visible"
        );
        is(
          content.document.querySelector(selectors.addressAutofillCheckbox)
            .checked,
          false,
          "Checkbox should be unchecked when Autofill Addresses is disabled"
        );
        is(
          content.document.querySelector(selectors.creditCardAutofillCheckbox)
            .checked,
          false,
          "Checkbox should be unchecked when Autofill Credit Cards is disabled"
        );
        content.document
          .querySelector(selectors.addressAutofillCheckbox)
          .scrollIntoView({ block: "center", behavior: "instant" });
      });

      info("test toggling the checkboxes");
      await BrowserTestUtils.synthesizeMouseAtCenter(
        SELECTORS.addressAutofillCheckbox,
        {},
        browser
      );
      is(
        Services.prefs.getBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF),
        true,
        "Check address autofill is now enabled"
      );

      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        content.document
          .querySelector(selectors.creditCardAutofillCheckbox)
          .scrollIntoView({ block: "center", behavior: "instant" });
      });
      await BrowserTestUtils.synthesizeMouseAtCenter(
        SELECTORS.creditCardAutofillCheckbox,
        {},
        browser
      );
      is(
        Services.prefs.getBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF),
        true,
        "Check credit card autofill is now enabled"
      );
    }
  );
});

add_task(async function test_creditCardNotAvailable() {
  await SpecialPowers.pushPrefEnv({
    set: [[AUTOFILL_CREDITCARDS_AVAILABLE_PREF, false]],
  });
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        is(
          content.document.querySelector(selectors.group).hidden,
          false,
          "Form Autofill group should be visible"
        );
        ok(
          !content.document.querySelector(selectors.creditCardAutofillCheckbox),
          "Autofill credit cards checkbox should not exist"
        );
      });
    }
  );
});

add_task(async function test_creditCardHiddenUI() {
  const AUTOFILL_CREDITCARDS_HIDE_UI_PREF =
    "extensions.formautofill.creditCards.hideui";

  await SpecialPowers.pushPrefEnv({
    set: [[AUTOFILL_CREDITCARDS_HIDE_UI_PREF, true]],
  });
  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(browser, [SELECTORS], selectors => {
        is(
          content.document.querySelector(selectors.group).hidden,
          false,
          "Form Autofill group should be visible"
        );
        ok(
          !content.document.querySelector(selectors.creditCardAutofillCheckbox),
          "Autofill credit cards checkbox should not exist"
        );
      });
    }
  );
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_HIDE_UI_PREF);
});

add_task(async function test_reauth() {
  await SpecialPowers.pushPrefEnv({
    set: [[AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true]],
  });
  let { OSKeyStore } = ChromeUtils.import(
    "resource://gre/modules/OSKeyStore.jsm"
  );

  let finalPrefPaneLoaded = TestUtils.topicObserved(
    "sync-pane-loaded",
    () => true
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_PRIVACY },
    async function(browser) {
      await finalPrefPaneLoaded;
      await SpecialPowers.spawn(
        browser,
        [SELECTORS, OSKeyStore.canReauth()],
        (selectors, canReauth) => {
          is(
            canReauth,
            !!content.document.querySelector(selectors.reauthCheckbox),
            "Re-authentication checkbox should be available if OSKeyStore.canReauth() is `true`"
          );
        }
      );
    }
  );
});
