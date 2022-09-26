/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { PageDataSchema } from "resource:///modules/pagedata/PageDataSchema.sys.mjs";

/**
 * Represents an item from the schema.org specification.
 *
 * Every `Item` has a type and a set of properties. Each property has a string
 * name and a list of values. It often isn't clear from the spec whether a
 * property is expected to have a list of values or just one value so this
 * data structure stores every property as a list and provides a simple method
 * to get the first property value.
 */
class Item {
  /** @type {string} The type of the item e.g. "Product" or "Person". */
  type;

  /** @type {Map<string, any[]>} Properties of the item. */
  properties = new Map();

  /**
   * Constructors a new `Item` of the given type.
   *
   * @param {string} type
   *   The type of the item.
   */
  constructor(type) {
    this.type = type;
  }

  /**
   * Tests whether a property has any values in this item.
   *
   * @param {string} prop
   *   The name of the property.
   * @returns {boolean}
   */
  has(prop) {
    return this.properties.has(prop);
  }

  /**
   * Gets all of the values for a property. This may return an empty array if
   * there are no values.
   *
   * @param {string} prop
   *   The name of the property.
   * @returns {any[]}
   */
  all(prop) {
    return this.properties.get(prop) ?? [];
  }

  /**
   * Gets the first value for a property.
   *
   * @param {string} prop
   *   The name of the property.
   * @returns {any}
   */
  get(prop) {
    return this.properties.get(prop)?.[0];
  }

  /**
   * Sets a value for a property.
   *
   * @param {string} prop
   *   The name of the property.
   * @param {any} value
   *   The value of the property.
   */
  set(prop, value) {
    let props = this.properties.get(prop);
    if (props === undefined) {
      props = [];
      this.properties.set(prop, props);
    }

    props.push(value);
  }

  /**
   * Converts this item to JSON-LD.
   *
   * Single array properties are converted into simple properties.
   *
   * @returns {object}
   */
  toJsonLD() {
    /**
     * Converts a value to its JSON-LD representation.
     *
     * @param {any} val
     *   The value to convert.
     * @returns {any}
     */
    function toLD(val) {
      if (val instanceof Item) {
        return val.toJsonLD();
      }
      return val;
    }

    let props = Array.from(this.properties, ([key, value]) => {
      if (value.length == 1) {
        return [key, toLD(value[0])];
      }

      return [key, value.map(toLD)];
    });

    return {
      "@type": this.type,
      ...Object.fromEntries(props),
    };
  }
}

/**
 * Parses the value for a given microdata property.
 * See https://html.spec.whatwg.org/multipage/microdata.html#values for the parsing spec
 *
 * @param {Element} propElement
 *   The property element.
 * @returns {any}
 *   The value of the property.
 */
function parseMicrodataProp(propElement) {
  if (propElement.hasAttribute("itemscope")) {
    throw new Error(
      "Cannot parse a simple property value from an itemscope element."
    );
  }

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

  switch (propElement.localName) {
    case "meta":
      return propElement.getAttribute("content") ?? "";
    case "audio":
    case "embed":
    case "iframe":
    case "source":
    case "track":
    case "video":
      return parseUrl(propElement, "src");
    case "img":
      // Some pages may be using a lazy loading approach to images, putting a
      // temporary image in "src" while the real image is in a differently
      // named attribute. So far we found "content" and "data-src" are common
      // names for that attribute.
      return (
        parseUrl(propElement, "content") ||
        parseUrl(propElement, "data-src") ||
        parseUrl(propElement, "src")
      );
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

/**
 * Collects product data from an item.
 *
 * @param {Document} document
 *   The document the item comes from.
 * @param {PageData} pageData
 *   The pageData object to add to.
 * @param {Item} item
 *   The product item.
 */
function collectProduct(document, pageData, item) {
  if (item.has("image")) {
    let url = new URL(item.get("image"), document.documentURI);
    pageData.image = url.toString();
  }

  if (item.has("description")) {
    pageData.description = item.get("description");
  }

  pageData.data[PageDataSchema.DATA_TYPE.PRODUCT] = {
    name: item.get("name"),
  };

  for (let offer of item.all("offers")) {
    if (!(offer instanceof Item) || offer.type != "Offer") {
      continue;
    }

    let price = parseFloat(offer.get("price"));
    if (!isNaN(price)) {
      pageData.data[PageDataSchema.DATA_TYPE.PRODUCT].price = {
        value: price,
        currency: offer.get("priceCurrency"),
      };

      break;
    }
  }
}

/**
 * Returns the root microdata items from the given document.
 *
 * @param {Document} document
 *   The DOM document to collect from.
 * @returns {Item[]}
 */
function collectMicrodataItems(document) {
  // First find all of the items in the document.
  let itemElements = document.querySelectorAll(
    "[itemscope][itemtype^='https://schema.org/'], [itemscope][itemtype^='http://schema.org/']"
  );

  /**
   * Maps elements to the closest item.
   *
   * @type {Map<Element, Item>}
   */
  let items = new Map();

  /**
   * Finds the item for an element. Throws if there is no item. Caches the
   * result.
   *
   * @param {Element} element
   *   The element to search from.
   * @returns {Item}
   */
  function itemFor(element) {
    let item = items.get(element);
    if (item) {
      return item;
    }

    if (!element.parentElement) {
      throw new Error("Element has no parent item.");
    }

    item = itemFor(element.parentElement);
    items.set(element, item);
    return item;
  }

  for (let element of itemElements) {
    let itemType = element.getAttribute("itemtype");
    // Strip off the base url
    if (itemType.startsWith("https://")) {
      itemType = itemType.substring(19);
    } else {
      itemType = itemType.substring(18);
    }

    items.set(element, new Item(itemType));
  }

  // The initial roots are just all the items.
  let roots = new Set(items.values());

  // Now find all item properties.
  let itemProps = document.querySelectorAll(
    "[itemscope][itemtype^='https://schema.org/'] [itemprop], [itemscope][itemtype^='http://schema.org/'] [itemprop]"
  );

  for (let element of itemProps) {
    // The item is always defined above the current element.
    let item = itemFor(element.parentElement);

    // The properties value is either a nested item or a simple value.
    let propValue = items.get(element) ?? parseMicrodataProp(element);
    item.set(element.getAttribute("itemprop"), propValue);

    if (propValue instanceof Item) {
      // This item belongs to another item and so is not a root item.
      roots.delete(propValue);
    }
  }

  return [...roots];
}

/**
 * Returns the root JSON-LD items from the given document.
 *
 * @param {Document} document
 *   The DOM document to collect from.
 * @returns {Item[]}
 */
function collectJsonLDItems(document) {
  /**
   * The root items.
   * @type {Item[]}
   */
  let items = [];

  /**
   * Converts a JSON-LD value into an Item if appropriate.
   *
   * @param {any} val
   *   The value to convert.
   * @returns {any}
   */
  function fromLD(val) {
    if (typeof val == "object" && "@type" in val) {
      let item = new Item(val["@type"]);

      for (let [prop, value] of Object.entries(val)) {
        // Ignore meta properties.
        if (prop.startsWith("@")) {
          continue;
        }

        if (!Array.isArray(value)) {
          value = [value];
        }

        item.properties.set(prop, value.map(fromLD));
      }

      return item;
    }

    return val;
  }

  let scripts = document.querySelectorAll("script[type='application/ld+json'");
  for (let script of scripts) {
    try {
      let content = JSON.parse(script.textContent);

      if (typeof content != "object") {
        continue;
      }

      if (!("@context" in content)) {
        continue;
      }

      if (
        content["@context"] != "http://schema.org" &&
        content["@context"] != "https://schema.org"
      ) {
        continue;
      }

      let item = fromLD(content);
      if (item instanceof Item) {
        items.push(item);
      }
    } catch (e) {
      // Unparsable content.
    }
  }

  return items;
}

/**
 * Collects schema.org related data from a page.
 *
 * Currently only supports HTML Microdata and JSON-LD formats, not RDFa.
 */
export const SchemaOrgPageData = {
  /**
   * Parses and collects the schema.org items from the given document.
   * The returned items are the roots, i.e. the top-level items, there may be
   * other items as nested properties.
   *
   * @param {Document} document
   *   The DOM document to parse.
   * @returns {Item[]}
   */
  collectItems(document) {
    return collectMicrodataItems(document).concat(collectJsonLDItems(document));
  },

  /**
   * Performs PageData collection from the given document.
   *
   * @param {Document} document
   *   The DOM document to collect from.
   * @returns {PageData}
   */
  collect(document) {
    let pageData = { data: {} };

    let items = this.collectItems(document);

    for (let item of items) {
      switch (item.type) {
        case "Product":
          if (!(PageDataSchema.DATA_TYPE.PRODUCT in pageData.data)) {
            collectProduct(document, pageData, item);
          }
          break;
        case "Organization":
          pageData.siteName = item.get("name");
          break;
      }
    }

    return pageData;
  },
};
