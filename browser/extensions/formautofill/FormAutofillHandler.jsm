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
   * String of the filled profile's guid.
   */
  filledProfileGUID: null,

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   */
  collectFormFields() {
    let fieldDetails = FormAutofillHeuristics.getFormInfo(this.form);
    this.fieldDetails = fieldDetails ? fieldDetails : [];
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

    this.filledProfileGUID = profile.guid;
    for (let fieldDetail of this.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. the focused input which is filled in FormFillController.
      // 2. a non-empty input field
      // 3. the invalid value set
      // 4. value already chosen in select element

      let element = fieldDetail.elementWeakRef.get();
      if (!element || element === focusedInput) {
        continue;
      }

      let value = profile[fieldDetail.fieldName];
      if (element instanceof Ci.nsIDOMHTMLInputElement && value) {
        if (element.value) {
          continue;
        }
        element.setUserInput(value);
      } else if (element instanceof Ci.nsIDOMHTMLSelectElement) {
        for (let option of element.options) {
          if (value === option.textContent || value === option.value) {
            // Do not change value if the option is already selected.
            // Use case for multiple select is not considered here.
            if (option.selected) {
              break;
            }
            // TODO: Using dispatchEvent does not 100% simulate select change.
            //       Should investigate further in Bug 1365895.
            option.selected = true;
            element.dispatchEvent(new Event("input", {"bubbles": true}));
            element.dispatchEvent(new Event("change", {"bubbles": true}));
            break;
          }
        }
      }
    }
  },

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   */
  previewFormFields(profile) {
    log.debug("preview profile in autofillFormFields:", profile);
    /*
    for (let fieldDetail of this.fieldDetails) {
      let value = profile[fieldDetail.fieldName] || "";

      // Skip the fields that already has text entered
      if (fieldDetail.element.value) {
        continue;
      }

      // TODO: Set highlight style and preview text.
    }
    */
  },

  clearPreviewedFormFields() {
    log.debug("clear previewed fields in:", this.form);
    /*
    for (let fieldDetail of this.fieldDetails) {
      // TODO: Clear preview text

      // We keep the highlight of all fields if this form has
      // already been auto-filled with a profile.
      if (this.filledProfileGUID == null) {
        // TODO: Remove highlight style
      }
    }
    */
  },

  /**
   * Return the profile that is converted from fieldDetails and only non-empty fields
   * are included.
   *
   * @returns {Object} The new profile that convert from details with trimmed result.
   */
  createProfile() {
    let profile = {};

    this.fieldDetails.forEach(detail => {
      let element = detail.elementWeakRef.get();
      // Remove the unnecessary spaces
      let value = element && element.value.trim();
      if (!value) {
        return;
      }

      profile[detail.fieldName] = value;
    });

    return profile;
  },
};
