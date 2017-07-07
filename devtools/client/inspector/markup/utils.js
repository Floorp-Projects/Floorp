/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;

function* getNames(x) {
  yield* Object.getOwnPropertyNames(x);
  yield* Object.getOwnPropertySymbols(x);
}

// Utility function to get own properties descriptor map.
function getOwnPropertyDescriptors(...objects) {
  let descriptors = {};
  for (let object of objects) {
    for (let name of getNames(object)) {
      descriptors[name] = getOwnPropertyDescriptor(object, name);
    }
  }
  return descriptors;
}

/**
 * Apply a 'flashed' background and foreground color to elements. Intended
 * to be used with flashElementOff as a way of drawing attention to an element.
 *
 * @param  {Node} backgroundElt
 *         The element to set the highlighted background color on.
 * @param  {Node} foregroundElt
 *         The element to set the matching foreground color on.
 *         Optional.  This will equal backgroundElt if not set.
 */
function flashElementOn(backgroundElt, foregroundElt = backgroundElt) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Make sure the animation class is not here
  backgroundElt.classList.remove("flash-out");

  // Change the background
  backgroundElt.classList.add("theme-bg-contrast");

  foregroundElt.classList.add("theme-fg-contrast");
  [].forEach.call(
    foregroundElt.querySelectorAll("[class*=theme-fg-color]"),
    span => span.classList.add("theme-fg-contrast")
  );
}

/**
 * Remove a 'flashed' background and foreground color to elements.
 * See flashElementOn.
 *
 * @param  {Node} backgroundElt
 *         The element to reomve the highlighted background color on.
 * @param  {Node} foregroundElt
 *         The element to remove the matching foreground color on.
 *         Optional.  This will equal backgroundElt if not set.
 */
function flashElementOff(backgroundElt, foregroundElt = backgroundElt) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Add the animation class to smoothly remove the background
  backgroundElt.classList.add("flash-out");

  // Remove the background
  backgroundElt.classList.remove("theme-bg-contrast");

  foregroundElt.classList.remove("theme-fg-contrast");
  [].forEach.call(
    foregroundElt.querySelectorAll("[class*=theme-fg-color]"),
    span => span.classList.remove("theme-fg-contrast")
  );
}

/**
 * Retrieve the available width between a provided element left edge and a container right
 * edge. This used can be used as a max-width for inplace-editor (autocomplete) widgets
 * replacing Editor elements of the the markup-view;
 */
function getAutocompleteMaxWidth(element, container) {
  let elementRect = element.getBoundingClientRect();
  let containerRect = container.getBoundingClientRect();
  return containerRect.right - elementRect.left - 2;
}

/**
 * Parse attribute names and values from a string.
 *
 * @param  {String} attr
 *         The input string for which names/values are to be parsed.
 * @param  {HTMLDocument} doc
 *         A document that can be used to test valid attributes.
 * @return {Array}
 *         An array of attribute names and their values.
 */
function parseAttributeValues(attr, doc) {
  attr = attr.trim();

  let parseAndGetNode = str => {
    return new DOMParser().parseFromString(str, "text/html").body.childNodes[0];
  };

  // Handle bad user inputs by appending a " or ' if it fails to parse without
  // them. Also note that a SVG tag is used to make sure the HTML parser
  // preserves mixed-case attributes
  let el = parseAndGetNode("<svg " + attr + "></svg>") ||
           parseAndGetNode("<svg " + attr + "\"></svg>") ||
           parseAndGetNode("<svg " + attr + "'></svg>");

  let div = doc.createElement("div");
  let attributes = [];
  for (let {name, value} of el.attributes) {
    // Try to set on an element in the document, throws exception on bad input.
    // Prevents InvalidCharacterError - "String contains an invalid character".
    try {
      div.setAttribute(name, value);
      attributes.push({ name, value });
    } catch (e) {
      // This may throw exceptions on bad input.
      // Prevents InvalidCharacterError - "String contains an invalid
      // character".
    }
  }

  return attributes;
}

/**
 * Truncate the string and add ellipsis to the middle of the string.
 */
function truncateString(str, maxLength) {
  if (!str || str.length <= maxLength) {
    return str;
  }

  return str.substring(0, Math.ceil(maxLength / 2)) +
         "…" +
         str.substring(str.length - Math.floor(maxLength / 2));
}

module.exports = {
  flashElementOn,
  flashElementOff,
  getAutocompleteMaxWidth,
  parseAttributeValues,
  truncateString,
};
