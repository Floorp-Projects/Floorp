/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
* Generate a hash that uniquely identifies a stylesheet or element style attribute.
*
* @param {Object} source
*        Information about a stylesheet or element style attribute:
*        {
*          type:  {String}
*                 One of "stylesheet" or "element".
*          index: {Number|String}
*                 Position of the styleshet in the list of stylesheets in the document.
*                 If `type` is "element", `index` is the generated selector which
*                 uniquely identifies the element in the document.
*          href:  {String|null}
*                 URL of the stylesheet or of the document when `type` is "element".
*                 If the stylesheet is inline, `href` is null.
*        }
* @return {String}
*/
function getSourceHash(source) {
  const { type, index, href = "inline" } = source;

  return `${type}${index}${href}`;
}

/**
* Generate a hash that uniquely identifies a CSS rule.
*
* @param {Object} ruleData
*        Information about a CSS rule:
*        {
*          selector:  {String}
*                     CSS selector text
*          ancestors: {Array}
*                     Flattened CSS rule tree of the rule's ancestors with the root rule
*                     at the beginning of the array and the leaf rule at the end.
*          ruleIndex: {Array}
*                     Indexes of each ancestor rule within its parent rule.
*        }
* @return {String}
*/
function getRuleHash(ruleData) {
  const { selector = "", ancestors = [], ruleIndex } = ruleData;
  const atRules = ancestors.reduce((acc, rule) => {
    acc += `${rule.typeName} ${(rule.conditionText || rule.name || rule.keyText)}`;
    return acc;
  }, "");

  return `${atRules}${selector}${ruleIndex}`;
}

module.exports.getSourceHash = getSourceHash;
module.exports.getRuleHash = getRuleHash;
