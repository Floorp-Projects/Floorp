/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const cssTokenizer  = require("devtools/sourceeditor/css-tokenizer");

/**
 * Returns the string enclosed in quotes
 */
function quoteString(string) {
  let hasDoubleQuotes = string.contains('"');
  let hasSingleQuotes = string.contains("'");

  if (hasDoubleQuotes && !hasSingleQuotes) {
    // In this case, no escaping required, just enclose in single-quotes
    return "'" + string + "'";
  }

  // In all other cases, enclose in double-quotes, and escape any double-quote
  // that may be in the string
  return '"' + string.replace(/"/g, '\"') + '"';
}

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
  let tokens = cssTokenizer(inputString);

  let declarations = [{name: "", value: "", priority: ""}];

  let current = "", hasBang = false, lastProp;
  for (let token of tokens) {
    lastProp = declarations[declarations.length - 1];

    if (token.tokenType === ":") {
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
    } else if (token.tokenType === ";") {
      lastProp.value = current.trim();
      current = "";
      hasBang = false;
      declarations.push({name: "", value: "", priority: ""});
    } else {
      switch(token.tokenType) {
        case "IDENT":
          if (token.value === "important" && hasBang) {
            lastProp.priority = "important";
            hasBang = false;
          } else {
            if (hasBang) {
              current += "!";
            }
            current += token.value;
          }
          break;
        case "WHITESPACE":
          current += " ";
          break;
        case "DIMENSION":
          current += token.repr;
          break;
        case "HASH":
          current += "#" + token.value;
          break;
        case "URL":
          current += "url(" + quoteString(token.value) + ")";
          break;
        case "FUNCTION":
          current += token.value + "(";
          break;
        case "(":
        case ")":
          current += token.tokenType;
          break;
        case "EOF":
          break;
        case "DELIM":
          if (token.value === "!") {
            hasBang = true;
          } else {
            current += token.value;
          }
          break;
        case "STRING":
          current += quoteString(token.value);
          break;
        case "{":
        case "}":
          current += token.tokenType;
          break;
        default:
          current += token.value;
          break;
      }
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
};
exports.parseDeclarations = parseDeclarations;

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
};
exports.parseSingleValue = parseSingleValue;
