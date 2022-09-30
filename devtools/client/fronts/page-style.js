/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  pageStyleSpec,
} = require("resource://devtools/shared/specs/page-style.js");

/**
 * PageStyleFront, the front object for the PageStyleActor
 */
class PageStyleFront extends FrontClassWithSpec(pageStyleSpec) {
  _attributesCache = new Map();

  constructor(conn, targetFront, parentFront) {
    super(conn, targetFront, parentFront);
    this.inspector = this.getParent();

    this._clearAttributesCache = this._clearAttributesCache.bind(this);
    this.on("stylesheet-updated", this._clearAttributesCache);
    this.walker.on("new-mutations", this._clearAttributesCache);
  }

  form(form) {
    this._form = form;
  }

  get walker() {
    return this.inspector.walker;
  }

  get supportsFontStretchLevel4() {
    return this._form.traits && this._form.traits.fontStretchLevel4;
  }

  get supportsFontStyleLevel4() {
    return this._form.traits && this._form.traits.fontStyleLevel4;
  }

  get supportsFontVariations() {
    return this._form.traits && this._form.traits.fontVariations;
  }

  get supportsFontWeightLevel4() {
    return this._form.traits && this._form.traits.fontWeightLevel4;
  }

  getMatchedSelectors(node, property, options) {
    return super.getMatchedSelectors(node, property, options).then(ret => {
      return ret.matched;
    });
  }

  async getApplied(node, options = {}) {
    const ret = await super.getApplied(node, options);
    return ret.entries;
  }

  addNewRule(node, pseudoClasses) {
    return super.addNewRule(node, pseudoClasses).then(ret => {
      return ret.entries[0];
    });
  }

  /**
   * Get an array of existing attribute values in a node document, given an attribute type.
   *
   * @param {String} search: A string to filter attribute value on.
   * @param {String} attributeType: The type of attribute we want to retrieve the values.
   * @param {Element} node: The element we want to get possible attributes for. This will
   *        be used to get the document where the search is happening.
   * @returns {Array<String>} An array of strings
   */
  async getAttributesInOwnerDocument(search, attributeType, node) {
    if (!attributeType) {
      throw new Error("`type` should not be empty");
    }

    if (!search) {
      return [];
    }

    const lcFilter = search.toLowerCase();

    // If the new filter includes the string that was used on our last trip to the server,
    // we can filter the cached results instead of calling the server again.
    if (
      this._attributesCache &&
      this._attributesCache.has(attributeType) &&
      search.startsWith(this._attributesCache.get(attributeType).search)
    ) {
      const cachedResults = this._attributesCache
        .get(attributeType)
        .results.filter(item => item.toLowerCase().startsWith(lcFilter));
      this.emitForTests(
        "getAttributesInOwnerDocument-cache-hit",
        cachedResults
      );
      return cachedResults;
    }

    const results = await super.getAttributesInOwnerDocument(
      search,
      attributeType,
      node
    );
    this._attributesCache.set(attributeType, { search, results });
    return results;
  }

  _clearAttributesCache() {
    this._attributesCache.clear();
  }
}

exports.PageStyleFront = PageStyleFront;
registerFront(PageStyleFront);
