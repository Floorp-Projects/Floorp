/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["QueryContext"];

/**
 * QueryContext defines a user's autocomplete input from within the Address Bar.
 * It supplements it with details of how the search results should be obtained
 * and what they consist of.
 */
class QueryContext {
  /**
   * Constructs the QueryContext instance.
   *
   * @param {object} options
   *   The initial options for QueryContext.
   * @param {string} options.searchString
   *   The string the user entered in autocomplete. Could be the empty string
   *   in the case of the user opening the popup via the mouse.
   * @param {number} options.lastKey
   *   The last key the user entered (as a key code). Could be null if the search
   *   was started via the mouse.
   * @param {boolean} options.isPrivate
   *   Set to true if this query was started from a private browsing window.
   * @param {number} options.maxResults
   *   The maximum number of results that will be displayed for this query.
   * @param {boolean} [options.autoFill]
   *   Whether or not to include autofill results. Optional, as this is normally
   *   set by the AddressBarController.
   */
  constructor(options = {}) {
    this._checkRequiredOptions(options, [
      "searchString",
      "lastKey",
      "maxResults",
      "isPrivate",
    ]);

    this.autoFill = !!options.autoFill;
  }

  /**
   * Checks the required options, saving them as it goes.
   *
   * @param {object} options The options object to check.
   * @param {array} optionNames The names of the options to check for.
   * @throws {Error} Throws if there is a missing option.
   */
  _checkRequiredOptions(options, optionNames) {
    for (let optionName of optionNames) {
      if (!(optionName in options)) {
        throw new Error(`Missing or empty ${optionName} provided to QueryContext`);
      }
      this[optionName] = options[optionName];
    }
  }
}
