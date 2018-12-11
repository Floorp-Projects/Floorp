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
   * @param {integer} matchType one of UrlbarUtils.MATCH_TYPE.* values
   * @param {integer} matchSource one of UrlbarUtils.MATCH_SOURCE.* values
   * @param {object} payload data for this match. A payload should always
   *        contain a way to extract a final url to visit. The url getter
   *        should have a case for each of the types.
   * @param {object} [payloadHighlights] payload highlights, if any. Each
   *        property in the payload may have a corresponding property in this
   *        object. The value of each property should be an array of [index,
   *        length] tuples. Each tuple indicates a substring in the correspoding
   *        payload property.
   */
  constructor(matchType, matchSource, payload, payloadHighlights = {}) {
    // Type describes the payload and visualization that should be used for
    // this match.
    if (!Object.values(UrlbarUtils.MATCH_TYPE).includes(matchType)) {
      throw new Error("Invalid match type");
    }
    this.type = matchType;

    // Source describes which data has been used to derive this match. In case
    // multiple sources are involved, use the more privacy restricted.
    if (!Object.values(UrlbarUtils.MATCH_SOURCE).includes(matchSource)) {
      throw new Error("Invalid match source");
    }
    this.source = matchSource;

    // The payload contains match data. Some of the data is common across
    // multiple types, but most of it will vary.
    if (!payload || (typeof payload != "object")) {
      throw new Error("Invalid match payload");
    }
    this.payload = payload;

    if (!payloadHighlights || (typeof payloadHighlights != "object")) {
      throw new Error("Invalid match payload highlights");
    }
    this.payloadHighlights = payloadHighlights;

    // Make sure every property in the payload has an array of highlights.  If a
    // payload property does not have a highlights array, then give it one now.
    // That way the consumer doesn't need to check whether it exists.
    for (let name in payload) {
      if (!(name in this.payloadHighlights)) {
        this.payloadHighlights[name] = [];
      }
    }
  }

  /**
   * Returns a title that could be used as a label for this match.
   * @returns {string} The label to show in a simplified title / url view.
   */
  get title() {
    return this._titleAndHighlights[0];
  }

  /**
   * Returns an array of highlights for the title.
   * @returns {array} The array of highlights.
   */
  get titleHighlights() {
    return this._titleAndHighlights[1];
  }

  /**
   * Returns an array [title, highlights].
   * @returns {array} The title and array of highlights.
   */
  get _titleAndHighlights() {
    switch (this.type) {
      case UrlbarUtils.MATCH_TYPE.TAB_SWITCH:
      case UrlbarUtils.MATCH_TYPE.URL:
      case UrlbarUtils.MATCH_TYPE.OMNIBOX:
      case UrlbarUtils.MATCH_TYPE.REMOTE_TAB:
        return this.payload.title ?
               [this.payload.title, this.payloadHighlights.title] :
               ["", []];
      case UrlbarUtils.MATCH_TYPE.SEARCH:
        return [this.payload.engine, this.payloadHighlights.engine];
      default:
        return ["", []];
    }
  }

  /**
   * Returns an icon url.
   * @returns {string} url of the icon.
   */
  get icon() {
    return this.payload.icon;
  }

  /**
   * A convenience function that takes a payload annotated with should-highlight
   * bools and returns the payload and the payload's highlights.  Use this
   * function when the highlighting required by your payload is based on simple
   * substring matching, as done by UrlbarUtils.getTokenMatches().  Pass the
   * return values as the `payload` and `payloadHighlights` params of the
   * UrlbarMatch constructor.
   *
   * @param {array} tokens The tokens that should be highlighted in each of the
   *        payload properties.
   * @param {object} payloadInfo An object that looks like this:
   *        {
   *          payloadPropertyName: [payloadPropertyValue, shouldHighlight],
   *          ...
   *        }
   * @returns {array} An array [payload, payloadHighlights].
   */
  static payloadAndSimpleHighlights(tokens, payloadInfo) {
    let entries = Object.entries(payloadInfo);
    return [
      entries.reduce((payload, [name, [val, _]]) => {
        payload[name] = val;
        return payload;
      }, {}),
      entries.reduce((highlights, [name, [val, shouldHighlight]]) => {
        if (shouldHighlight) {
          highlights[name] =
            !Array.isArray(val) ?
            UrlbarUtils.getTokenMatches(tokens, val || "") :
            val.map(subval => UrlbarUtils.getTokenMatches(tokens, subval));
        }
        return highlights;
      }, {}),
    ];
  }
}
