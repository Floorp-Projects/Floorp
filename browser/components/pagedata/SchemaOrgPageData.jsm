/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SchemaOrgPageData"];

const { PageDataCollector } = ChromeUtils.import(
  "resource:///modules/pagedata/PageDataCollector.jsm"
);

/**
 * @typedef {object} ProductData
 *   Data about a product.
 * @property {string | undefined} gtin
 *   The Global Trade Item Number for the product.
 * @property {string | undefined} name
 *   The name of the product.
 * @property {URL | undefined} url
 *   The url of the product.
 * @property {string | undefined} image
 *   the url of a product image.
 * @property {string | undefined} price
 *   The price of the product.
 * @property {string | undefined} currency
 *   The currency of the price.
 */

/**
 * Finds the values for a given property.
 * See https://html.spec.whatwg.org/multipage/microdata.html#values for the parsing spec
 *
 * TODO: Currently this will find item properties of inner-items. Need to use itemscope as a
 *   boundary.
 *
 * @param {Element} element
 *   The item scope.
 * @param {string} prop
 *   The property to find.
 * @returns {any[]}
 *   The value of the property.
 */
function getProp(element, prop) {
  const parseUrl = (urlElement, attr) => {
    if (!urlElement.hasAttribute(attr)) {
      return "";
    }

    try {
      let url = new URL(
        urlElement.getAttribute(attr),
        urlElement.ownerDocument.documentURI
      );
      return url.toString();
    } catch (e) {
      return "";
    }
  };

  return Array.from(
    // Ignores properties that are scopes.
    element.querySelectorAll(`[itemprop~='${prop}']:not([itemscope])`),
    propElement => {
      switch (propElement.localName) {
        case "meta":
          return propElement.getAttribute("content") ?? "";
        case "audio":
        case "embed":
        case "iframe":
        case "img":
        case "source":
        case "track":
        case "video":
          return parseUrl(propElement, "src");
        case "object":
          return parseUrl(propElement, "data");
        case "a":
        case "area":
        case "link":
          return parseUrl(propElement, "href");
        case "data":
        case "meter":
          return propElement.getAttribute("value");
        case "time":
          if (propElement.hasAtribute("datetime")) {
            return propElement.getAttribute("datetime");
          }
          return propElement.textContent;
        default:
          // Not mentioned in the spec but sites seem to use it.
          if (propElement.hasAttribute("content")) {
            return propElement.getAttribute("content");
          }
          return propElement.textContent;
      }
    }
  );
}

/**
 * Collects schema.org related data from a page.
 *
 * Currently only supports HTML Microdata, not RDFa or JSON-LD formats.
 * Currently only collects product data.
 *
 * TODO: Respond to DOM mutations to trigger recollection.
 */
class SchemaOrgPageData extends PageDataCollector {
  /**
   * @see PageDataCollector.init
   */
  async init() {
    return this.#collect();
  }

  /**
   * Collects product data from an element.
   *
   * @param {Element} element
   *   The DOM element representing the product.
   *
   * @returns {ProductData}
   *   The product data.
   */
  #collectProduct(element) {
    // At the moment we simply grab the first element found for each property.
    // In future we may need to do something better.
    return {
      gtin: getProp(element, "gtin")[0],
      name: getProp(element, "name")[0],
      image: getProp(element, "image")[0] || undefined,
      url: getProp(element, "url")[0] || undefined,
      price: getProp(element, "price")[0],
      currency: getProp(element, "priceCurrency")[0],
    };
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

    let scopes = this.document.querySelectorAll(
      "[itemscope][itemtype^='https://schema.org/'], [itemscope][itemtype^='http://schema.org/']"
    );

    for (let scope of scopes) {
      let itemType = scope.getAttribute("itemtype");
      // Strip off the protocol
      if (itemType.startsWith("https://")) {
        itemType = itemType.substring(8);
      } else {
        itemType = itemType.substring(7);
      }

      switch (itemType) {
        case "schema.org/Product":
          insert(
            PageDataCollector.DATA_TYPE.PRODUCT,
            this.#collectProduct(scope)
          );
          break;
      }
    }

    return Array.from(items, ([type, data]) => ({ type, data }));
  }
}
