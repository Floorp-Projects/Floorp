/**
 * Test enabling the feature in specific locales and regions.
 */

"use strict";

const DOM_ENABLED_PREF = "dom.forms.autocomplete.formautofill";

add_task(async function test_defaultTestEnvironment() {
  Assert.ok(Services.prefs.getBoolPref(DOM_ENABLED_PREF));
});

add_task(async function test_unsupportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "ZZ");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.ok(!Services.prefs.getBoolPref(DOM_ENABLED_PREF));
});

add_task(async function test_supportedRegion() {
  Services.prefs.setCharPref("extensions.formautofill.available", "detect");
  Services.prefs.setCharPref("browser.search.region", "US");
  registerCleanupFunction(function cleanupRegion() {
    Services.prefs.clearUserPref("browser.search.region");
  });

  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.ok(Services.prefs.getBoolPref(DOM_ENABLED_PREF));
});
