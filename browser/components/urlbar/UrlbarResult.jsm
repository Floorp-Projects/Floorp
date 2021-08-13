/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module exports a urlbar result class, each representing a single result
 * found by a provider that can be passed from the model to the view through
 * the controller. It is mainly defined by a result type, and a payload,
 * containing the data. A few getters allow to retrieve information common to all
 * the result types.
 */

var EXPORTED_SYMBOLS = ["UrlbarResult"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUIUtils: "resource:///modules/BrowserUIUtils.jsm",
  JsonSchemaValidator:
    "resource://gre/modules/components-utils/JsonSchemaValidator.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

/**
 * Class used to create a single result.
 */
class UrlbarResult {
  /**
   * Creates a result.
   * @param {integer} resultType one of UrlbarUtils.RESULT_TYPE.* values
   * @param {integer} resultSource one of UrlbarUtils.RESULT_SOURCE.* values
   * @param {object} payload data for this result. A payload should always
   *        contain a way to extract a final url to visit. The url getter
   *        should have a case for each of the types.
   * @param {object} [payloadHighlights] payload highlights, if any. Each
   *        property in the payload may have a corresponding property in this
   *        object. The value of each property should be an array of [index,
   *        length] tuples. Each tuple indicates a substring in the correspoding
   *        payload property.
   */
  constructor(resultType, resultSource, payload, payloadHighlights = {}) {
    // Type describes the payload and visualization that should be used for
    // this result.
    if (!Object.values(UrlbarUtils.RESULT_TYPE).includes(resultType)) {
      throw new Error("Invalid result type");
    }
    this.type = resultType;

    // Source describes which data has been used to derive this result. In case
    // multiple sources are involved, use the more privacy restricted.
    if (!Object.values(UrlbarUtils.RESULT_SOURCE).includes(resultSource)) {
      throw new Error("Invalid result source");
    }
    this.source = resultSource;

    // UrlbarView is responsible for updating this.
    this.rowIndex = -1;

    // May be used to indicate an heuristic result. Heuristic results can bypass
    // source filters in the ProvidersManager, that otherwise may skip them.
    this.heuristic = false;

    // The payload contains result data. Some of the data is common across
    // multiple types, but most of it will vary.
    if (!payload || typeof payload != "object") {
      throw new Error("Invalid result payload");
    }
    this.payload = this.validatePayload(payload);

    if (!payloadHighlights || typeof payloadHighlights != "object") {
      throw new Error("Invalid result payload highlights");
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
   * Returns a title that could be used as a label for this result.
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
      case UrlbarUtils.RESULT_TYPE.KEYWORD:
      case UrlbarUtils.RESULT_TYPE.TAB_SWITCH:
      case UrlbarUtils.RESULT_TYPE.URL:
      case UrlbarUtils.RESULT_TYPE.OMNIBOX:
      case UrlbarUtils.RESULT_TYPE.REMOTE_TAB:
        if (this.payload.qsSuggestion) {
          return [
            // We will initially only be targetting en-US users with this experiment
            // but will need to change this to work properly with l10n.
            this.payload.qsSuggestion + " — " + this.payload.title,
            this.payloadHighlights.qsSuggestion,
          ];
        }
        return this.payload.title
          ? [this.payload.title, this.payloadHighlights.title]
          : [this.payload.url || "", this.payloadHighlights.url || []];
      case UrlbarUtils.RESULT_TYPE.SEARCH:
        if (this.payload.providesSearchMode) {
          return ["", []];
        }
        if (this.payload.tail && this.payload.tailOffsetIndex >= 0) {
          return [this.payload.tail, this.payloadHighlights.tail];
        } else if (this.payload.suggestion) {
          return [this.payload.suggestion, this.payloadHighlights.suggestion];
        }
        return [this.payload.query, this.payloadHighlights.query];
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
   * Returns whether the result's `suggestedIndex` property is defined.
   * `suggestedIndex` is an optional hint to the muxer that can be set to
   * suggest a specific position among the results.
   * @returns {boolean} Whether `suggestedIndex` is defined.
   */
  get hasSuggestedIndex() {
    return typeof this.suggestedIndex == "number";
  }

  /**
   * Returns the given payload if it's valid or throws an error if it's not.
   * The schemas in UrlbarUtils.RESULT_PAYLOAD_SCHEMA are used for validation.
   *
   * @param {object} payload The payload object.
   * @returns {object} `payload` if it's valid.
   */
  validatePayload(payload) {
    let schema = UrlbarUtils.getPayloadSchema(this.type);
    if (!schema) {
      throw new Error(`Unrecognized result type: ${this.type}`);
    }
    let result = JsonSchemaValidator.validate(payload, schema, {
      allowExplicitUndefinedProperties: true,
      allowNullAsUndefinedProperties: true,
      allowExtraProperties: this.type == UrlbarUtils.RESULT_TYPE.DYNAMIC,
    });
    if (!result.valid) {
      throw result.error;
    }
    return payload;
  }

  /**
   * A convenience function that takes a payload annotated with
   * UrlbarUtils.HIGHLIGHT enums and returns the payload and the payload's
   * highlights. Use this function when the highlighting required by your
   * payload is based on simple substring matching, as done by
   * UrlbarUtils.getTokenMatches(). Pass the return values as the `payload` and
   * `payloadHighlights` params of the UrlbarResult constructor.
   * `payloadHighlights` is optional. If omitted, payload will not be
   * highlighted.
   *
   * If the payload doesn't have a title or has an empty title, and it also has
   * a URL, then this function also sets the title to the URL's domain.
   *
   * @param {array} tokens The tokens that should be highlighted in each of the
   *        payload properties.
   * @param {object} payloadInfo An object that looks like this:
   *        { payloadPropertyName: payloadPropertyInfo }
   *
   *        Each payloadPropertyInfo may be either a string or an array.  If
   *        it's a string, then the property value will be that string, and no
   *        highlighting will be applied to it.  If it's an array, then it
   *        should look like this: [payloadPropertyValue, highlightType].
   *        payloadPropertyValue may be a string or an array of strings.  If
   *        it's a string, then the payloadHighlights in the return value will
   *        be an array of match highlights as described in
   *        UrlbarUtils.getTokenMatches().  If it's an array, then
   *        payloadHighlights will be an array of arrays of match highlights,
   *        one element per element in payloadPropertyValue.
   * @returns {array} An array [payload, payloadHighlights].
   */
  static payloadAndSimpleHighlights(tokens, payloadInfo) {
    // Convert scalar values in payloadInfo to [value] arrays.
    for (let [name, info] of Object.entries(payloadInfo)) {
      if (!Array.isArray(info)) {
        payloadInfo[name] = [info];
      }
    }

    if (
      (!payloadInfo.title || !payloadInfo.title[0]) &&
      payloadInfo.url &&
      typeof payloadInfo.url[0] == "string"
    ) {
      // If there's no title, show the domain as the title.  Not all valid URLs
      // have a domain.
      payloadInfo.title = payloadInfo.title || [
        "",
        UrlbarUtils.HIGHLIGHT.TYPED,
      ];
      try {
        payloadInfo.title[0] = new URL(payloadInfo.url[0]).host;
      } catch (e) {}
    }

    if (payloadInfo.url) {
      // For display purposes we need to unescape the url.
      payloadInfo.displayUrl = [...payloadInfo.url];
      let url = payloadInfo.displayUrl[0];
      if (url && UrlbarPrefs.get("trimURLs")) {
        url = BrowserUIUtils.removeSingleTrailingSlashFromURL(url);
        if (url.startsWith("https://")) {
          url = url.substring(8);
          if (url.startsWith("www.")) {
            url = url.substring(4);
          }
        }
      }
      payloadInfo.displayUrl[0] = UrlbarUtils.unEscapeURIForUI(url);
    }

    // For performance reasons limit excessive string lengths, to reduce the
    // amount of string matching we do here, and avoid wasting resources to
    // handle long textruns that the user would never see anyway.
    for (let prop of ["displayUrl", "title", "suggestion"]) {
      let val = payloadInfo[prop]?.[0];
      if (typeof val == "string") {
        payloadInfo[prop][0] = val.substring(0, UrlbarUtils.MAX_TEXT_LENGTH);
      }
    }

    let entries = Object.entries(payloadInfo);
    return [
      entries.reduce((payload, [name, [val, _]]) => {
        payload[name] = val;
        return payload;
      }, {}),
      entries.reduce((highlights, [name, [val, highlightType]]) => {
        if (highlightType) {
          highlights[name] = !Array.isArray(val)
            ? UrlbarUtils.getTokenMatches(tokens, val || "", highlightType)
            : val.map(subval =>
                UrlbarUtils.getTokenMatches(tokens, subval, highlightType)
              );
        }
        return highlights;
      }, {}),
    ];
  }

  static _dynamicResultTypesByName = new Map();

  /**
   * Registers a dynamic result type.  Dynamic result types are types that are
   * created at runtime, for example by an extension.  A particular type should
   * be added only once; if this method is called for a type more than once, the
   * `type` in the last call overrides those in previous calls.
   *
   * @param {string} name
   *   The name of the type.  This is used in CSS selectors, so it shouldn't
   *   contain any spaces or punctuation except for -, _, etc.
   * @param {object} type
   *   An object that describes the type.  Currently types do not have any
   *   associated metadata, so this object should be empty.
   */
  static addDynamicResultType(name, type = {}) {
    if (/[^a-z0-9_-]/i.test(name)) {
      Cu.reportError(`Illegal dynamic type name: ${name}`);
      return;
    }
    this._dynamicResultTypesByName.set(name, type);
  }

  /**
   * Unregisters a dynamic result type.
   *
   * @param {string} name
   *   The name of the type.
   */
  static removeDynamicResultType(name) {
    let type = this._dynamicResultTypesByName.get(name);
    if (type) {
      this._dynamicResultTypesByName.delete(name);
    }
  }

  /**
   * Returns an object describing a registered dynamic result type.
   *
   * @param {string} name
   *   The name of the type.
   * @returns {object}
   *   Currently types do not have any associated metadata, so the return value
   *   is an empty object if the type exists.  If the type doesn't exist,
   *   undefined is returned.
   */
  static getDynamicResultType(name) {
    return this._dynamicResultTypesByName.get(name);
  }

  /**
   * This is useful for logging results. If you need the full payload, then it's
   * better to JSON.stringify the result object itself.
   * @returns {string} string representation of the result.
   */
  toString() {
    if (this.payload.url) {
      return this.payload.title + " - " + this.payload.url.substr(0, 100);
    }
    if (this.payload.keyword) {
      return this.payload.keyword + " - " + this.payload.query;
    }
    if (this.payload.suggestion) {
      return this.payload.engine + " - " + this.payload.suggestion;
    }
    if (this.payload.engine) {
      return this.payload.engine + " - " + this.payload.query;
    }
    return JSON.stringify(this);
  }
}
