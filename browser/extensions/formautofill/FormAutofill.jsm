/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormAutofill"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const ADDRESSES_FIRST_TIME_USE_PREF = "extensions.formautofill.firstTimeUse";
const AUTOFILL_CREDITCARDS_AVAILABLE_PREF = "extensions.formautofill.creditCards.available";
const CREDITCARDS_USED_STATUS_PREF = "extensions.formautofill.creditCards.used";
const DEFAULT_REGION_PREF = "browser.search.region";
const ENABLED_AUTOFILL_ADDRESSES_PREF = "extensions.formautofill.addresses.enabled";
const ENABLED_AUTOFILL_CREDITCARDS_PREF = "extensions.formautofill.creditCards.enabled";
const SUPPORTED_COUNTRIES_PREF = "extensions.formautofill.supportedCountries";

XPCOMUtils.defineLazyPreferenceGetter(this, "logLevel", "extensions.formautofill.loglevel",
                                      "Warn");

// A logging helper for debug logging to avoid creating Console objects
// or triggering expensive JS -> C++ calls when debug logging is not
// enabled.
//
// Console objects, even natively-implemented ones, can consume a lot of
// memory, and since this code may run in every content process, that
// memory can add up quickly. And, even when debug-level messages are
// being ignored, console.debug() calls can be expensive.
//
// This helper avoids both of those problems by never touching the
// console object unless debug logging is enabled.
function debug() {
  if (logLevel == "debug") {
    this.log.debug(...arguments);
  }
}

var FormAutofill = {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  ADDRESSES_FIRST_TIME_USE_PREF,
  CREDITCARDS_USED_STATUS_PREF,

  get isAutofillEnabled() { return FormAutofill.isAutofillAddressesEnabled || this.isAutofillCreditCardsEnabled; },
  get isAutofillCreditCardsEnabled() { return FormAutofill.isAutofillCreditCardsAvailable && FormAutofill._isAutofillCreditCardsEnabled; },

  defineLazyLogGetter(scope, logPrefix) {
    scope.debug = debug;

    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
      return new ConsoleAPI({
        maxLogLevelPref: "extensions.formautofill.loglevel",
        prefix: logPrefix,
      });
    });
  },
};

XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "DEFAULT_REGION", DEFAULT_REGION_PREF, "US");
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "isAutofillAddressesEnabled", ENABLED_AUTOFILL_ADDRESSES_PREF);
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "isAutofillCreditCardsAvailable", AUTOFILL_CREDITCARDS_AVAILABLE_PREF);
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "_isAutofillCreditCardsEnabled", ENABLED_AUTOFILL_CREDITCARDS_PREF);
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "isAutofillAddressesFirstTimeUse", ADDRESSES_FIRST_TIME_USE_PREF);
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "AutofillCreditCardsUsedStatus", CREDITCARDS_USED_STATUS_PREF);
XPCOMUtils.defineLazyPreferenceGetter(FormAutofill,
                                      "supportedCountries", SUPPORTED_COUNTRIES_PREF, null, null,
                                      val => val.split(","));
