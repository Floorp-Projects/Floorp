/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Form Autofill field heuristics.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHeuristics"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/**
 * Returns the autocomplete information of fields according to heuristics.
 */
this.FormAutofillHeuristics = {
  VALID_FIELDS: [
    "given-name",
    "additional-name",
    "family-name",
    "organization",
    "street-address",
    "address-line1",
    "address-line2",
    "address-line3",
    "address-level2",
    "address-level1",
    "postal-code",
    "country",
    "tel",
    "email",
  ],

  getInfo(element) {
    if (!(element instanceof Ci.nsIDOMHTMLInputElement)) {
      return null;
    }

    let info = element.getAutocompleteInfo();
    if (!info || !info.fieldName ||
        !this.VALID_FIELDS.includes(info.fieldName)) {
      return null;
    }

    return info;
  },
};
