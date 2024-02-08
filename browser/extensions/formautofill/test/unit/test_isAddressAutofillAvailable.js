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

  Assert.equal(
    Services.prefs.getBoolPref(
      "extensions.formautofill.addresses.experiments.enabled"
    ),
    true
  );
});

add_task(async function test_default_supported_address_region() {
  let addon = await AddonManager.getAddonByID(EXTENSION_ID);
  await addon.reload();

  Assert.equal(FormAutofill.isAutofillAddressesAvailable, true);
  Assert.equal(FormAutofill.isAutofillAddressesEnabled, true);
});

add_task(async function test_unsupported_address_region() {
  const addon = await AddonManager.getAddonByID(EXTENSION_ID);

  Services.prefs.setBoolPref(
    "extensions.formautofill.addresses.experiments.enabled",
    false
  );

  await addon.reload();
  Assert.equal(FormAutofill.isAutofillAddressesAvailable, false);
  Assert.equal(FormAutofill.isAutofillAddressesEnabled, false);
});
