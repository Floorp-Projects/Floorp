/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ProfileAutoCompleteResult"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.ProfileAutoCompleteResult = function(searchString,
                                           fieldName,
                                           matchingProfiles,
                                           {resultCode = null}) {
  this.searchString = searchString;
  this._fieldName = fieldName;
  this._matchingProfiles = matchingProfiles;

  if (resultCode) {
    this.searchResult = resultCode;
  } else if (matchingProfiles.length > 0) {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
  } else {
    this.searchResult = Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
  }
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

  // The autocomplete attribute of the focused input field
  _fieldName: "",

  // The matching profiles contains the information for filling forms.
  _matchingProfiles: null,

  /**
   * @returns {number} The number of results
   */
  get matchCount() {
    return this._matchingProfiles.length;
  },

  _checkIndexBounds(index) {
    if (index < 0 || index >= this._matchingProfiles.length) {
      throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  },

  /**
   * Retrieves a result
   * @param   {number} index The index of the result requested
   * @returns {string} The result at the specified index
   */
  getValueAt(index) {
    this._checkIndexBounds(index);
    return this._matchingProfiles[index].guid;
  },

  getLabelAt(index) {
    this._checkIndexBounds(index);
    return this._matchingProfiles[index].organization;
  },

  /**
   * Retrieves a comment (metadata instance)
   * @param   {number} index The index of the comment requested
   * @returns {string} The comment at the specified index
   */
  getCommentAt(index) {
    this._checkIndexBounds(index);
    return this._matchingProfiles[index].streetAddress;
  },

  /**
   * Retrieves a style hint specific to a particular index.
   * @param   {number} index The index of the style hint requested
   * @returns {string} The style hint at the specified index
   */
  getStyleAt(index) {
    this._checkIndexBounds(index);
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
