/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormAutofillUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
);

// Check whether os auth is disabled by default on a new profile in Beta and Release.
add_task(async function test_creditCards_os_auth_disabled_for_new_profile() {
  Assert.equal(
    FormAutofillUtils.getOSAuthEnabled(
      FormAutofillUtils.AUTOFILL_CREDITCARDS_REAUTH_PREF
    ),
    AppConstants.NIGHTLY_BUILD,
    "OS Auth should be disabled for credit cards by default for a new profile."
  );

  Assert.equal(
    LoginHelper.getOSAuthEnabled(LoginHelper.OS_AUTH_FOR_PASSWORDS_PREF),
    AppConstants.NIGHTLY_BUILD,
    "OS Auth should be disabled for passwords by default for a new profile."
  );
});
