/**
 * Test enabling the feature in specific locales and regions.
 */

"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);

add_task(async function test_defaultTestEnvironment() {
  Assert.ok(Services.prefs.getBoolPref("dom.forms.autocomplete.formautofill"));
});

add_task(async function test_detect_unsupportedRegion() {
  Services.prefs.setCharPref(
    "extensions.formautofill.creditCards.supported",
    "detect"
  );
  Services.prefs.setCharPref(
    "extensions.formautofill.creditCards.supportedCountries",
    "US,CA"
  );
  Services.prefs.setCharPref("browser.search.region", "ZZ");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
    Services.prefs.clearUserPref(
      "extensions.formautofill.creditCards.supported"
    );
    Services.prefs.clearUserPref("extensions.formautofill.addresses.supported");
    Services.prefs.clearUserPref(
      "extensions.formautofill.creditCards.supportedCountries"
    );
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.equal(
    FormAutofill.isAutofillCreditCardsAvailable,
    false,
    "Credit card autofill should not be available"
  );
  Assert.equal(
    FormAutofill.isAutofillCreditCardsEnabled,
    false,
    "Credit card autofill should not be enabled"
  );
});

add_task(async function test_detect_supportedRegion() {
  Services.prefs.setCharPref(
    "extensions.formautofill.creditCards.supported",
    "detect"
  );
  Services.prefs.setCharPref(
    "extensions.formautofill.creditCards.supportedCountries",
    "US,CA"
  );
  Services.prefs.setCharPref("browser.search.region", "US");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
    Services.prefs.clearUserPref(
      "extensions.formautofill.creditCards.supported"
    );
    Services.prefs.clearUserPref(
      "extensions.formautofill.creditCards.supportedCountries"
    );
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.equal(
    FormAutofill.isAutofillCreditCardsAvailable,
    true,
    "Credit card autofill should be available"
  );
  Assert.equal(
    FormAutofill.isAutofillCreditCardsEnabled,
    true,
    "Credit card autofill should be enabled"
  );
});
