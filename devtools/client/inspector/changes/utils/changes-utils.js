/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getFormatStr, getStr } = require("./l10n");

/**
 * Generate a hash that uniquely identifies a stylesheet or element style attribute.
 *
 * @param {Object} source
 *        Information about a stylesheet or element style attribute:
 *        {
 *          type:  {String}
 *                 One of "stylesheet", "inline" or "element".
 *          index: {Number|String}
 *                 Position of the styleshet in the list of stylesheets in the document.
 *                 If `type` is "element", `index` is the generated selector which
 *                 uniquely identifies the element in the document.
 *          href:  {String}
 *                 URL of the stylesheet or of the document when `type` is "element" or
 *                 "inline".
 *        }
 * @return {String}
 */
function getSourceHash(source) {
  const { type, index, href } = source;

  return `${type}${index}${href}`;
}

/**
 * Generate a hash that uniquely identifies a CSS rule.
 *
 * @param {Object} ruleData
 *        Information about a CSS rule:
 *        {
 *          selectors: {Array}
 *                     Array of CSS selector text
 *          ancestors: {Array}
 *                     Flattened CSS rule tree of the rule's ancestors with the root rule
 *                     at the beginning of the array and the leaf rule at the end.
 *          ruleIndex: {Array}
 *                     Indexes of each ancestor rule within its parent rule.
 *        }
 * @return {String}
 */
function getRuleHash(ruleData) {
  const { selectors = [], ancestors = [], ruleIndex } = ruleData;
  const atRules = ancestors.reduce((acc, rule) => {
    acc += `${rule.typeName} ${rule.conditionText ||
      rule.name ||
      rule.keyText}`;
    return acc;
  }, "");

  return `${atRules}${selectors}${ruleIndex}`;
}

/**
 * Get a human-friendly style source path to display in the Changes panel.
 * For element inline styles, return a string indicating that.
 * For inline stylesheets, return a string indicating that plus the stylesheet's index.
 * For URLs, return just the stylesheet filename.
 *
 * @param {Object} source
 *        Information about the style source. Contains:
 *        - type: {String} One of "element" or "stylesheet"
 *        - href: {String|null} Stylesheet URL or document URL for elmeent inline styles
 *        - index: {Number} Position of the stylesheet in its document's stylesheet list.
 * @return {String}
 */
function getSourceForDisplay(source) {
  let href;

  switch (source.type) {
    case "element":
      href = getStr("changes.elementStyleLabel");
      break;
    case "inline":
      href = getFormatStr("changes.inlineStyleSheetLabel", `#${source.index}`);
      break;
    case "stylesheet":
      const url = new URL(source.href);
      href = url.pathname.substring(url.pathname.lastIndexOf("/") + 1);
      break;
  }

  return href;
}

module.exports.getSourceForDisplay = getSourceForDisplay;
module.exports.getSourceHash = getSourceHash;
module.exports.getRuleHash = getRuleHash;
