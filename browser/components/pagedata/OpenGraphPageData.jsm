/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["OpenGraphPageData"];

const { PageDataCollector } = ChromeUtils.import(
  "resource:///modules/pagedata/PageDataCollector.jsm"
);

/**
 * @typedef {object} GeneralPageData
 *   Data about a product.
 * @property {string | undefined} title
 *   The title describing the page.
 * @property {string | undefined} site_name
 *   The name of the site the page is on.
 * @property {string | undefined} type
 *   The type of the object being described by Open Graph. See
 *   https://ogp.me/#types for a list of possible types.
 * @property {string | undefined} image
 *   A URL pointing to an image that describes the page.
 * @property {string | undefined} url
 *   The permalink to the page.
 */

const RELEVANT_TAGS = ["title", "site_name", "image", "type", "url"];

/**
 * Collects Open Graph related data from a page.
 *
 * TODO: Respond to DOM mutations to trigger recollection.
 */
class OpenGraphPageData extends PageDataCollector {
  /**
   * @see PageDataCollector.init
   */
  async init() {
    return this.#collect();
  }

  /**
   * Collects data from the meta tags on the page.
   * See https://ogp.me/ for the parsing spec.
   *
   * @param {NodeList} tags
   *  A NodeList of Open Graph meta tags.
   * @returns {GeneralPageData}
   *   Data describing the webpage.
   */
  #collectOpenGraphTags(tags) {
    // Ensure all tags are present in the returned object, even if their values
    // are undefined.
    let pageData = Object.fromEntries(
      RELEVANT_TAGS.map(tag => [tag, undefined])
    );

    for (let tag of tags) {
      // Stripping "og:" from the property name.
      let propertyName = tag.getAttribute("property").substring(3);
      if (RELEVANT_TAGS.includes(propertyName)) {
        pageData[propertyName] = tag.getAttribute("content");
      }
    }

    return pageData;
  }

  /**
   * Collects the existing data from the page.
   *
   * @returns {Data[]}
   */
  #collect() {
    /**
     * A map from item type to an array of the items found in the page.
     */
    let items = new Map();
    let insert = (type, item) => {
      let data = items.get(type);
      if (!data) {
        data = [];
        items.set(type, data);
      }
      data.push(item);
    };

    // Sites can technically define an Open Graph prefix other than `og:`.
    // However, `og:` is one of the default RDFa prefixes and it's likely
    // uncommon that sites use a custom prefix. If we find that metadata is
    // missing for common sites due to this issue, we could consider adding a
    // basic RDFa parser.
    let openGraphTags = this.document.querySelectorAll("meta[property^='og:'");
    if (!openGraphTags.length) {
      return [];
    }
    insert(
      PageDataCollector.DATA_TYPE.GENERAL,
      this.#collectOpenGraphTags(openGraphTags)
    );

    return Array.from(items, ([type, data]) => ({ type, data }));
  }
}
