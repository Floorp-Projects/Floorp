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
    "name",
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

  getFormInfo(form) {
    let fieldDetails = [];
    for (let element of form.elements) {
      // Exclude elements to which no autocomplete field has been assigned.
      let info = this.getInfo(element);
      if (!info) {
        continue;
      }

      // Store the association between the field metadata and the element.
      if (fieldDetails.some(f => f.section == info.section &&
                                 f.addressType == info.addressType &&
                                 f.contactType == info.contactType &&
                                 f.fieldName == info.fieldName)) {
        // A field with the same identifier already exists.
        log.debug("Not collecting a field matching another with the same info:", info);
        continue;
      }

      let formatWithElement = {
        section: info.section,
        addressType: info.addressType,
        contactType: info.contactType,
        fieldName: info.fieldName,
        elementWeakRef: Cu.getWeakReference(element),
      };

      fieldDetails.push(formatWithElement);
    }

    return fieldDetails;
  },

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
