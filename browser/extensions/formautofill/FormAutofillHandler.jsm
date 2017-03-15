/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHandler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHeuristics",
                                  "resource://formautofill/FormAutofillHeuristics.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

/**
 * Handles profile autofill for a DOM Form element.
 * @param {FormLike} form Form that need to be auto filled
 */
function FormAutofillHandler(form) {
  this.form = form;
  this.fieldDetails = [];
}

FormAutofillHandler.prototype = {
  /**
   * DOM Form element to which this object is attached.
   */
  form: null,

  /**
   * Array of collected data about relevant form fields.  Each item is an object
   * storing the identifying details of the field and a reference to the
   * originally associated element from the form.
   *
   * The "section", "addressType", "contactType", and "fieldName" values are
   * used to identify the exact field when the serializable data is received
   * from the backend.  There cannot be multiple fields which have
   * the same exact combination of these values.
   *
   * A direct reference to the associated element cannot be sent to the user
   * interface because processing may be done in the parent process.
   */
  fieldDetails: null,

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   */
  collectFormFields() {
    this.fieldDetails = [];

    for (let element of this.form.elements) {
      // Exclude elements to which no autocomplete field has been assigned.
      let info = FormAutofillHeuristics.getInfo(element);
      if (!info) {
        continue;
      }

      // Store the association between the field metadata and the element.
      if (this.fieldDetails.some(f => f.section == info.section &&
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
        element, // TODO: Apply Cu.getWeakReference and use get API for strong ref.
      };

      this.fieldDetails.push(formatWithElement);
    }

    log.debug("Collected details on", this.fieldDetails.length, "fields");
  },

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @param {Object} focusedInput
   *        A focused input element which is skipped for filling.
   */
  autofillFormFields(profile, focusedInput) {
    log.debug("profile in autofillFormFields:", profile);
    for (let fieldDetail of this.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. the focused input which is filled in FormFillController.
      // 2. a non-empty input field
      // 3. the invalid value set

      if (fieldDetail.element === focusedInput ||
          fieldDetail.element.value) {
        continue;
      }

      let value = profile[fieldDetail.fieldName];
      if (value) {
        fieldDetail.element.setUserInput(value);
      }
    }
  },
};
