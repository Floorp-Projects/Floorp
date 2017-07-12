/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ProfileAutoCompleteResult"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://formautofill/FormAutofillUtils.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);


this.ProfileAutoCompleteResult = function(searchString,
                                          focusedFieldName,
                                          allFieldNames,
                                          matchingProfiles,
                                          {resultCode = null}) {
  log.debug("Constructing new ProfileAutoCompleteResult:", [...arguments]);
  this.searchString = searchString;
  this._focusedFieldName = focusedFieldName;
  this._allFieldNames = allFieldNames;
  this._matchingProfiles = matchingProfiles;

  if (resultCode) {
    this.searchResult = resultCode;
  } else if (matchingProfiles.length > 0) {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
  } else {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }

  this._popupLabels = this._generateLabels(this._focusedFieldName,
                                           this._allFieldNames,
                                           this._matchingProfiles);
  // Add an empty result entry for footer. Its content will come from
  // the footer binding, so don't assign any value to it.
  this._popupLabels.push({
    primary: "",
    secondary: "",
    categories: FormAutofillUtils.getCategoriesFromFieldNames(allFieldNames),
    focusedCategory: FormAutofillUtils.getCategoryFromFieldName(focusedFieldName),
  });
};

ProfileAutoCompleteResult.prototype = {

  // The user's query string
  searchString: "",

  // The default item that should be entered if none is selected
  defaultIndex: 0,

  // The reason the search failed
  errorDescription: "",

  // The result code of this result object.
  searchResult: null,

  // The field name of the focused input.
  _focusedFieldName: "",

  // All field names in the form which contains the focused input.
  _allFieldNames: null,

  // The matching profiles contains the information for filling forms.
  _matchingProfiles: null,

  // An array of primary and secondary labels for each profiles.
  _popupLabels: null,

  /**
   * @returns {number} The number of results
   */
  get matchCount() {
    return this._popupLabels.length;
  },

  _checkIndexBounds(index) {
    if (index < 0 || index >= this._popupLabels.length) {
      throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  },

  /**
   * Get the secondary label based on the focused field name and related field names
   * in the same form.
   * @param   {string} focusedFieldName The field name of the focused input
   * @param   {Array<Object>} allFieldNames The field names in the same section
   * @param   {object} profile The profile providing the labels to show.
   * @returns {string} The secondary label
   */
  _getSecondaryLabel(focusedFieldName, allFieldNames, profile) {
    // We group similar fields into the same field name so we won't pick another
    // field in the same group as the secondary label.
    const GROUP_FIELDS = {
      "name": [
        "name",
        "given-name",
        "additional-name",
        "family-name",
      ],
      "street-address": [
        "street-address",
        "address-line1",
        "address-line2",
        "address-line3",
      ],
      "country-name": [
        "country",
        "country-name",
      ],
    };

    const secondaryLabelOrder = [
      "street-address",  // Street address
      "name",            // Full name
      "address-level2",  // City/Town
      "organization",    // Company or organization name
      "address-level1",  // Province/State (Standardized code if possible)
      "country-name",    // Country name
      "postal-code",     // Postal code
      "tel",             // Phone number
      "email",           // Email address
    ];

    for (let field in GROUP_FIELDS) {
      if (GROUP_FIELDS[field].includes(focusedFieldName)) {
        focusedFieldName = field;
        break;
      }
    }

    for (const currentFieldName of secondaryLabelOrder) {
      if (focusedFieldName == currentFieldName || !profile[currentFieldName]) {
        continue;
      }

      let matching = GROUP_FIELDS[currentFieldName] ?
        allFieldNames.some(fieldName => GROUP_FIELDS[currentFieldName].includes(fieldName)) :
        allFieldNames.includes(currentFieldName);

      if (matching) {
        if (currentFieldName == "street-address" &&
            profile["-moz-street-address-one-line"]) {
          return profile["-moz-street-address-one-line"];
        }
        return profile[currentFieldName];
      }
    }

    return ""; // Nothing matched.
  },

  _generateLabels(focusedFieldName, allFieldNames, profiles) {
    // Skip results without a primary label.
    return profiles.filter(profile => {
      return !!profile[focusedFieldName];
    }).map(profile => {
      let primaryLabel = profile[focusedFieldName];
      if (focusedFieldName == "street-address" &&
          profile["-moz-street-address-one-line"]) {
        primaryLabel = profile["-moz-street-address-one-line"];
      }
      return {
        primary: primaryLabel,
        secondary: this._getSecondaryLabel(focusedFieldName,
                                           allFieldNames,
                                           profile),
      };
    });
  },

  /**
   * Retrieves a result
   * @param   {number} index The index of the result requested
   * @returns {string} The result at the specified index
   */
  getValueAt(index) {
    this._checkIndexBounds(index);
    return this._popupLabels[index].primary;
  },

  getLabelAt(index) {
    this._checkIndexBounds(index);
    return JSON.stringify(this._popupLabels[index]);
  },

  /**
   * Retrieves a comment (metadata instance)
   * @param   {number} index The index of the comment requested
   * @returns {string} The comment at the specified index
   */
  getCommentAt(index) {
    this._checkIndexBounds(index);
    return JSON.stringify(this._matchingProfiles[index]);
  },

  /**
   * Retrieves a style hint specific to a particular index.
   * @param   {number} index The index of the style hint requested
   * @returns {string} The style hint at the specified index
   */
  getStyleAt(index) {
    this._checkIndexBounds(index);
    if (index == this.matchCount - 1) {
      return "autofill-footer";
    }
    return "autofill-profile";
  },

  /**
   * Retrieves an image url.
   * @param   {number} index The index of the image url requested
   * @returns {string} The image url at the specified index
   */
  getImageAt(index) {
    this._checkIndexBounds(index);
    return "";
  },

  /**
   * Retrieves a result
   * @param   {number} index The index of the result requested
   * @returns {string} The result at the specified index
   */
  getFinalCompleteValueAt(index) {
    return this.getValueAt(index);
  },

  /**
   * Removes a result from the resultset
   * @param {number} index The index of the result to remove
   * @param {boolean} removeFromDatabase TRUE for removing data from DataBase
   *                                     as well.
   */
  removeValueAt(index, removeFromDatabase) {
    // There is no plan to support removing profiles via autocomplete.
  },

  // nsISupports
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult]),
};
