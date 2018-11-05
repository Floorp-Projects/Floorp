/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a urlbar match class, each representing a single match.
 * A match is a single search result found by a provider, that can be passed
 * from the model to the view, through the controller. It is mainly defined by
 * a type of the match, and a payload, containing the data. A few getters allow
 * to retrieve information common to all the match types.
 */

var EXPORTED_SYMBOLS = ["UrlbarMatch"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create a match.
 */
class UrlbarMatch {
  /**
   * Creates a match.
   * @param {integer} matchType one of UrlbarUtils.MATCHTYPE.* values
   * @param {object} payload data for this match. A payload should always
   *        contain a way to extract a final url to visit. The url getter
   *        should have a case for each of the types.
   */
  constructor(matchType, payload) {
    this.type = matchType;
    this.payload = payload;
  }

  /**
   * Returns a final destination for this match.
   * Different kind of matches may have different ways to express this value,
   * and this is a common getter for all of them.
   * @returns {string} a url to load when this match is confirmed byt the user.
   */
  get url() {
    switch (this.type) {
      case UrlbarUtils.MATCH_TYPE.TAB_SWITCH:
        return this.payload.url;
    }
    return "";
  }

  /**
   * Returns a title that could be used as a label for this match.
   * @returns {string} The label to show in a simplified title / url view.
   */
  get title() {
    return "";
  }
}
