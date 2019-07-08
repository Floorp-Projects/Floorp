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
  getAddressLabel(address, addressFields = null) {
    if (addressFields) {
      let requestedFields = addressFields.trim().split(/\s+/);
      return (
        requestedFields
          .filter(f => f && address[f])
          .map(f => address[f])
          .join(", ") + ` (${address.guid})`
      );
    }
    return `${address.name} (${address.guid})`;
  },

  getCreditCardNetworks() {
    // Shim for list of known and supported credit card network ids as exposed by
    // toolkit/modules/CreditCard.jsm
    return [
      "amex",
      "cartebancaire",
      "diners",
      "discover",
      "jcb",
      "mastercard",
      "mir",
      "unionpay",
      "visa",
    ];
  },
  isCCNumber(str) {
    return !!str.replace(/[-\s]/g, "").match(/^\d{9,}$/);
  },
  DEFAULT_REGION: "US",
  countries: new Map([
    ["US", "United States"],
    ["CA", "Canada"],
    ["DE", "Germany"],
  ]),
  getFormFormat(country) {
    if (country == "DE") {
      return {
        addressLevel3Label: "suburb",
        addressLevel2Label: "city",
        addressLevel1Label: "province",
        addressLevel1Options: null,
        postalCodeLabel: "postalCode",
        fieldsOrder: [
          {
            fieldId: "name",
            newLine: true,
          },
          {
            fieldId: "organization",
            newLine: true,
          },
          {
            fieldId: "street-address",
            newLine: true,
          },
          { fieldId: "postal-code" },
          { fieldId: "address-level2" },
        ],
        postalCodePattern: "\\d{5}",
        countryRequiredFields: [
          "street-address",
          "address-level2",
          "postal-code",
        ],
      };
    }

    let addressLevel1Options = null;
    if (country == "US") {
      addressLevel1Options = new Map([
        ["CA", "California"],
        ["MA", "Massachusetts"],
        ["MI", "Michigan"],
      ]);
    } else if (country == "CA") {
      addressLevel1Options = new Map([
        ["NS", "Nova Scotia"],
        ["ON", "Ontario"],
        ["YT", "Yukon"],
      ]);
    }

    let fieldsOrder = [
      { fieldId: "name", newLine: true },
      { fieldId: "street-address", newLine: true },
      { fieldId: "address-level2" },
      { fieldId: "address-level1" },
      { fieldId: "postal-code" },
      { fieldId: "organization" },
    ];
    if (country == "BR") {
      fieldsOrder.splice(2, 0, { fieldId: "address-level3" });
    }

    return {
      addressLevel3Label: "suburb",
      addressLevel2Label: "city",
      addressLevel1Label: country == "US" ? "state" : "province",
      addressLevel1Options,
      postalCodeLabel: country == "US" ? "zip" : "postalCode",
      fieldsOrder,
      // The following values come from addressReferences.js and should not be changed.
      /* eslint-disable-next-line max-len */
      postalCodePattern:
        country == "US"
          ? "(\\d{5})(?:[ \\-](\\d{4}))?"
          : "[ABCEGHJKLMNPRSTVXY]\\d[ABCEGHJ-NPRSTV-Z] ?\\d[ABCEGHJ-NPRSTV-Z]\\d",
      countryRequiredFields:
        country == "US" || country == "CA"
          ? [
              "street-address",
              "address-level2",
              "address-level1",
              "postal-code",
            ]
          : ["street-address", "address-level2", "postal-code"],
    };
  },
  findAddressSelectOption(selectEl, address, fieldName) {
    return null;
  },
  getDefaultPreferences() {
    let prefValues = {
      saveCreditCardDefaultChecked: false,
      saveAddressDefaultChecked: true,
    };
    return prefValues;
  },
  isOfficialBranding() {
    return false;
  },
};
