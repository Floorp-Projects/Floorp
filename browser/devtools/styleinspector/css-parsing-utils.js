/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {cssTokenizer} = require("devtools/sourceeditor/css-tokenizer");

const SELECTOR_ATTRIBUTE = exports.SELECTOR_ATTRIBUTE = 1;
const SELECTOR_ELEMENT = exports.SELECTOR_ELEMENT = 2;
const SELECTOR_PSEUDO_CLASS = exports.SELECTOR_PSEUDO_CLASS = 3;

/**
 * Returns an array of CSS declarations given an string.
 * For example, parseDeclarations("width: 1px; height: 1px") would return
 * [{name:"width", value: "1px"}, {name: "height", "value": "1px"}]
 *
 * The input string is assumed to only contain declarations so { and } characters
 * will be treated as part of either the property or value, depending where it's
 * found.
 *
 * @param {string} inputString
 *        An input string of CSS
 * @return {Array} an array of objects with the following signature:
 *         [{"name": string, "value": string, "priority": string}, ...]
 */
function parseDeclarations(inputString) {
  if (inputString === null || inputString === undefined) {
    throw new Error("empty input string");
  }

  let tokens = cssTokenizer(inputString);

  let declarations = [{name: "", value: "", priority: ""}];

  let current = "", hasBang = false, lastProp;
  for (let token of tokens) {
    lastProp = declarations[declarations.length - 1];

    if (token.tokenType === "symbol" && token.text === ":") {
      if (!lastProp.name) {
        // Set the current declaration name if there's no name yet
        lastProp.name = current.trim();
        current = "";
        hasBang = false;
      } else {
        // Otherwise, just append ':' to the current value (declaration value
        // with colons)
        current += ":";
      }
    } else if (token.tokenType === "symbol" && token.text === ";") {
      lastProp.value = current.trim();
      current = "";
      hasBang = false;
      declarations.push({name: "", value: "", priority: ""});
    } else if (token.tokenType === "ident") {
      if (token.text === "important" && hasBang) {
        lastProp.priority = "important";
        hasBang = false;
      } else {
        if (hasBang) {
          current += "!";
        }
        current += token.text;
      }
    } else if (token.tokenType === "symbol" && token.text === "!") {
      hasBang = true;
    } else if (token.tokenType === "whitespace") {
      current += " ";
    } else if (token.tokenType === "comment") {
      // For now, just ignore.
    } else {
      current += inputString.substring(token.startOffset, token.endOffset);
    }
  }

  // Handle whatever trailing properties or values might still be there
  if (current) {
    if (!lastProp.name) {
      // Trailing property found, e.g. p1:v1;p2:v2;p3
      lastProp.name = current.trim();
    } else {
      // Trailing value found, i.e. value without an ending ;
      lastProp.value += current.trim();
    }
  }

  // Remove declarations that have neither a name nor a value
  declarations = declarations.filter(prop => prop.name || prop.value);

  return declarations;
}

/**
 * Returns an array of the parsed CSS selector value and type given a string.
 *
 * The components making up the CSS selector can be extracted into 3 different
 * types: element, attribute and pseudoclass. The object that is appended to
 * the returned array contains the value related to one of the 3 types described
 * along with the actual type.
 *
 * The following are the 3 types that can be returned in the object signature:
 * (1) SELECTOR_ATTRIBUTE
 * (2) SELECTOR_ELEMENT
 * (3) SELECTOR_PSEUDO_CLASS
 *
 * @param {string} value
 *        The CSS selector text.
 * @return {Array} an array of objects with the following signature:
 *         [{ "value": string, "type": integer }, ...]
 */
function parsePseudoClassesAndAttributes(value) {
  if (!value) {
    throw new Error("empty input string");
  }

  let tokens = cssTokenizer(value);
  let result = [];
  let current = "";
  let functionCount = 0;
  let hasAttribute = false;
  let hasColon = false;

  for (let token of tokens) {
    if (token.tokenType === "ident") {
      current += value.substring(token.startOffset, token.endOffset);

      if (hasColon && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_PSEUDO_CLASS });
        }

        current = "";
        hasColon = false;
      }
    } else if (token.tokenType === "symbol" && token.text === ":") {
      if (!hasColon) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ELEMENT });
        }

        current = "";
        hasColon = true;
      }

      current += token.text;
    } else if (token.tokenType === "function") {
      current += value.substring(token.startOffset, token.endOffset);
      functionCount++;
    } else if (token.tokenType === "symbol" && token.text === ")") {
      current += token.text;

      if (hasColon && functionCount == 1) {
        if (current) {
          result.push({ value: current, type: SELECTOR_PSEUDO_CLASS });
        }

        current = "";
        functionCount--;
        hasColon = false;
      } else {
        functionCount--;
      }
    } else if (token.tokenType === "symbol" && token.text === "[") {
      if (!hasAttribute && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ELEMENT });
        }

        current = "";
        hasAttribute = true;
      }

      current += token.text;
    } else if (token.tokenType === "symbol" && token.text === "]") {
      current += token.text;

      if (hasAttribute && !functionCount) {
        if (current) {
          result.push({ value: current, type: SELECTOR_ATTRIBUTE });
        }

        current = "";
        hasAttribute = false;
      }
    } else {
      current += value.substring(token.startOffset, token.endOffset);
    }
  }

  if (current) {
    result.push({ value: current, type: SELECTOR_ELEMENT });
  }

  return result;
}

/**
 * Expects a single CSS value to be passed as the input and parses the value
 * and priority.
 *
 * @param {string} value The value from the text editor.
 * @return {object} an object with 'value' and 'priority' properties.
 */
function parseSingleValue(value) {
  let declaration = parseDeclarations("a: " + value + ";")[0];
  return {
    value: declaration ? declaration.value : "",
    priority: declaration ? declaration.priority : ""
  };
}

exports.parseDeclarations = parseDeclarations;
exports.parsePseudoClassesAndAttributes = parsePseudoClassesAndAttributes;
exports.parseSingleValue = parseSingleValue;
