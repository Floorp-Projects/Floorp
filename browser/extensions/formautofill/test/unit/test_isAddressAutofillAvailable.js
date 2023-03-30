/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test enabling address autofill in specific locales and regions.
 */

"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);

add_task(async function test_defaultTestEnvironment() {
  Assert.equal(
    Services.prefs.getCharPref("extensions.formautofill.addresses.supported"),
    "on"
  );
});

add_task(async function test_default_supported_module_and_autofill_region() {
  Services.prefs.setCharPref("browser.search.region", "US");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.equal(FormAutofill.isAutofillAddressesAvailable, true);
  Assert.equal(FormAutofill.isAutofillAddressesEnabled, true);
});

add_task(
  async function test_supported_creditCard_region_unsupported_address_region() {
    Services.prefs.setCharPref(
      "extensions.formautofill.addresses.supported",
      "detect"
    );
    Services.prefs.setCharPref(
      "extensions.formautofill.creditCards.supported",
      "detect"
    );
    Services.prefs.setCharPref("browser.search.region", "FR");
    Services.prefs.setCharPref(
      "extensions.formautofill.addresses.supportedCountries",
      "US,CA"
    );
    Services.prefs.setCharPref(
      "extensions.formautofill.creditCards.supportedCountries",
      "US,CA,FR"
    );
    registerCleanupFunction(function cleanupPrefs() {
      Services.prefs.clearUserPref("browser.search.region");
      Services.prefs.clearUserPref(
        "extensions.formautofill.addresses.supportedCountries"
      );
      Services.prefs.clearUserPref(
        "extensions.formautofill.addresses.supported"
      );
      Services.prefs.clearUserPref(
        "extensions.formautofill.creditCards.supported"
      );
    });

    let addon = await AddonManager.getAddonByID(EXTENSION_ID);
    await addon.reload();
    Assert.ok(
      Services.prefs.getBoolPref("extensions.formautofill.creditCards.enabled")
    );
    Assert.equal(FormAutofill.isAutofillAddressesAvailable, false);
    Assert.equal(FormAutofill.isAutofillAddressesEnabled, false);
  }
);
