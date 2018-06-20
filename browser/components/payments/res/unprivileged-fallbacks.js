/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file defines fallback objects to be used during development outside
 * of the paymentDialogWrapper. When loaded in the wrapper, a frame script
 * overwrites these methods. Since these methods need to get overwritten in the
 * global scope, it can't be converted into an ES module.
 */

/* eslint-disable no-console */
/* exported log, PaymentDialogUtils */

"use strict";

var log = {
  error: console.error.bind(console, "paymentRequest.xhtml:"),
  warn: console.warn.bind(console, "paymentRequest.xhtml:"),
  info: console.info.bind(console, "paymentRequest.xhtml:"),
  debug: console.debug.bind(console, "paymentRequest.xhtml:"),
};

var PaymentDialogUtils = {
  getAddressLabel(address) {
    return `${address.name} (${address.guid})`;
  },
  isCCNumber(str) {
    return str.length > 0;
  },
  DEFAULT_REGION: "US",
  supportedCountries: ["US", "CA"],
  getFormFormat(country) {
    return {
      "addressLevel1Label": country == "US" ? "state" : "province",
      "postalCodeLabel": country == "US" ? "zip" : "postalCode",
      "fieldsOrder": [
        {fieldId: "name", newLine: true},
        {fieldId: "organization", newLine: true},
        {fieldId: "street-address", newLine: true},
        {fieldId: "address-level2"},
        {fieldId: "address-level1"},
        {fieldId: "postal-code"},
      ],
      // The following values come from addressReferences.js and should not be changed.
      /* eslint-disable-next-line max-len */
      "postalCodePattern": country == "US" ? "(\\d{5})(?:[ \\-](\\d{4}))?" : "[ABCEGHJKLMNPRSTVXY]\\d[ABCEGHJ-NPRSTV-Z] ?\\d[ABCEGHJ-NPRSTV-Z]\\d",
    };
  },
};
