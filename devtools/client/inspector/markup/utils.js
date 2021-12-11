/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Apply a 'flashed' background and foreground color to elements. Intended
 * to be used with flashElementOff as a way of drawing attention to an element.
 *
 * @param  {Node} backgroundElt
 *         The element to set the highlighted background color on.
 * @param  {Object} options
 * @param  {Node} options.foregroundElt
 *         The element to set the matching foreground color on. This will equal
 *         backgroundElt if not set.
 * @param  {String} options.backgroundClass
 *         The background highlight color class to set on the element.
 */
function flashElementOn(
  backgroundElt,
  { foregroundElt = backgroundElt, backgroundClass = "theme-bg-contrast" } = {}
) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Make sure the animation class is not here
  backgroundElt.classList.remove("flash-out");

  // Change the background
  backgroundElt.classList.add(backgroundClass);

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
 *         The element to remove the highlighted background color on.
 * @param  {Object} options
 * @param  {Node} options.foregroundElt
 *         The element to remove the matching foreground color on. This will equal
 *         backgroundElt if not set.
 * @param  {String} options.backgroundClass
 *         The background highlight color class to remove on the element.
 */
function flashElementOff(
  backgroundElt,
  { foregroundElt = backgroundElt, backgroundClass = "theme-bg-contrast" } = {}
) {
  if (!backgroundElt || !foregroundElt) {
    return;
  }

  // Add the animation class to smoothly remove the background
  backgroundElt.classList.add("flash-out");

  // Remove the background
  backgroundElt.classList.remove(backgroundClass);

  foregroundElt.classList.remove("theme-fg-contrast");
  // Make sure the foreground animation class is removed
  foregroundElt.classList.remove("flash-out");
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
  const elementRect = element.getBoundingClientRect();
  const containerRect = container.getBoundingClientRect();
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

  const parseAndGetNode = str => {
    return new DOMParser().parseFromString(str, "text/html").body.childNodes[0];
  };

  // Handle bad user inputs by appending a " or ' if it fails to parse without
  // them. Also note that a SVG tag is used to make sure the HTML parser
  // preserves mixed-case attributes
  const el =
    parseAndGetNode("<svg " + attr + "></svg>") ||
    parseAndGetNode("<svg " + attr + '"></svg>') ||
    parseAndGetNode("<svg " + attr + "'></svg>");

  const div = doc.createElement("div");
  const attributes = [];
  for (const { name, value } of el.attributes) {
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

module.exports = {
  flashElementOn,
  flashElementOff,
  getAutocompleteMaxWidth,
  parseAttributeValues,
};
