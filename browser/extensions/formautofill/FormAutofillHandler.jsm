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

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHeuristics",
                                  "resource://formautofill/FormAutofillHeuristics.jsm");

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
   * Returns information from the form about fields that can be autofilled, and
   * populates the fieldDetails array on this object accordingly.
   *
   * @returns {Array<Object>} Serializable data structure that can be sent to the user
   *          interface, or null if the operation failed because the constraints
   *          on the allowed fields were not honored.
   */
  collectFormFields() {
    let autofillData = [];

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
        return null;
      }

      let inputFormat = {
        section: info.section,
        addressType: info.addressType,
        contactType: info.contactType,
        fieldName: info.fieldName,
      };
      // Clone the inputFormat for caching the fields and elements together
      let formatWithElement = Object.assign({}, inputFormat);

      inputFormat.index = autofillData.length;
      autofillData.push(inputFormat);

      formatWithElement.element = element;
      this.fieldDetails.push(formatWithElement);
    }

    return autofillData;
  },

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * data provided by backend.
   *
   * @param {Array<Object>} autofillResult
   *        Data returned by the user interface.
   *        [{
   *          section: Value originally provided to the user interface.
   *          addressType: Value originally provided to the user interface.
   *          contactType: Value originally provided to the user interface.
   *          fieldName: Value originally provided to the user interface.
   *          value: String with which the field should be updated.
   *          index: Index to match the input in fieldDetails
   *        }],
   *        }
   */
  autofillFormFields(autofillResult) {
    for (let field of autofillResult) {
      // TODO: Skip filling the value of focused input which is filled in
      // FormFillController.

      // Get the field details, if it was processed by the user interface.
      let fieldDetail = this.fieldDetails[field.index];

      // Avoid the invalid value set
      if (!fieldDetail || !field.value) {
        continue;
      }

      let info = FormAutofillHeuristics.getInfo(fieldDetail.element);
      if (!info ||
          field.section != info.section ||
          field.addressType != info.addressType ||
          field.contactType != info.contactType ||
          field.fieldName != info.fieldName) {
        Cu.reportError("Autocomplete tokens mismatched");
        continue;
      }

      fieldDetail.element.setUserInput(field.value);
    }
  },
};
