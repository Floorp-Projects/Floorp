/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SchemaOrgPageData"];

const { PageDataSchema } = ChromeUtils.import(
  "resource:///modules/pagedata/PageDataSchema.jsm"
);

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
 * Collects product data from an item scope.
 *
 * @param {PageData} pageData
 *   The pageData object to add to.
 * @param {Element} element
 *   The product item scope element.
 */
function collectProduct(pageData, element) {
  // At the moment we simply grab the first element found for each property.
  // In future we may need to do something better.

  let images = getProp(element, "image");
  if (images.length) {
    pageData.image = images[0];
  }

  let descriptions = getProp(element, "description");
  if (descriptions.length) {
    pageData.description = descriptions[0];
  }

  pageData.data[PageDataSchema.DATA_TYPE.PRODUCT] = {
    name: getProp(element, "name")[0],
  };

  let prices = getProp(element, "price");
  if (prices.length) {
    let price = parseFloat(prices[0]);
    if (!isNaN(price)) {
      pageData.data[PageDataSchema.DATA_TYPE.PRODUCT].price = {
        value: price,
        currency: getProp(element, "priceCurrency")[0],
      };
    }
  }
}

/**
 * Collects schema.org related data from a page.
 *
 * Currently only supports HTML Microdata, not RDFa or JSON-LD formats.
 * Currently only collects product data.
 */
const SchemaOrgPageData = {
  collect(document) {
    let pageData = { data: {} };

    let scopes = document.querySelectorAll(
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
          if (!(PageDataSchema.DATA_TYPE.PRODUCT in pageData.data)) {
            collectProduct(pageData, scope);
          }
          break;
        case "schema.org/Organization":
          pageData.siteName = getProp(scope, "name")[0];
          break;
      }
    }

    return pageData;
  },
};
