/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// List of JSON content types.
const contentTypes = {
  "text/plain": 1,
  "text/javascript": 1,
  "text/x-javascript": 1,
  "text/json": 1,
  "text/x-json": 1,
  "application/json": 1,
  "application/x-json": 1,
  "application/javascript": 1,
  "application/x-javascript": 1,
  "application/json-rpc": 1
};

// Implementation
var Json = {};

/**
 * Parsing JSON
 */
Json.parseJSONString = function (jsonString) {
  if (!jsonString.length) {
    return null;
  }

  let regex, matches;

  let first = firstNonWs(jsonString);
  if (first !== "[" && first !== "{") {
    // This (probably) isn't pure JSON. Let's try to strip various sorts
    // of XSSI protection/wrapping and see if that works better.

    // Prototype-style secure requests
    regex = /^\s*\/\*-secure-([\s\S]*)\*\/\s*$/;
    matches = regex.exec(jsonString);
    if (matches) {
      jsonString = matches[1];

      if (jsonString[0] === "\\" && jsonString[1] === "n") {
        jsonString = jsonString.substr(2);
      }

      if (jsonString[jsonString.length - 2] === "\\" &&
        jsonString[jsonString.length - 1] === "n") {
        jsonString = jsonString.substr(0, jsonString.length - 2);
      }
    }

    // Google-style (?) delimiters
    if (jsonString.indexOf("&&&START&&&") !== -1) {
      regex = /&&&START&&&([\s\S]*)&&&END&&&/;
      matches = regex.exec(jsonString);
      if (matches) {
        jsonString = matches[1];
      }
    }

    // while(1);, for(;;);, and )]}'
    regex = /^\s*(\)\]\}[^\n]*\n|while\s*\(1\);|for\s*\(;;\);)([\s\S]*)/;
    matches = regex.exec(jsonString);
    if (matches) {
      jsonString = matches[2];
    }

    // JSONP
    regex = /^\s*([A-Za-z0-9_$.]+\s*(?:\[.*\]|))\s*\(([\s\S]*)\)/;
    matches = regex.exec(jsonString);
    if (matches) {
      jsonString = matches[2];
    }
  }

  try {
    return JSON.parse(jsonString);
  } catch (err) {
    // eslint-disable-line no-empty
  }

  // Give up if we don't have valid start, to avoid some unnecessary overhead.
  first = firstNonWs(jsonString);
  if (first !== "[" && first !== "{" && isNaN(first) && first !== '"') {
    return null;
  }

  // Remove JavaScript comments, quote non-quoted identifiers, and merge
  // multi-line structures like |{"a": 1} \n {"b": 2}| into a single JSON
  // object [{"a": 1}, {"b": 2}].
  jsonString = pseudoJsonToJson(jsonString);

  try {
    return JSON.parse(jsonString);
  } catch (err) {
    // eslint-disable-line no-empty
  }

  return null;
};

function firstNonWs(str) {
  for (let i = 0, len = str.length; i < len; i++) {
    let ch = str[i];
    if (ch !== " " && ch !== "\n" && ch !== "\t" && ch !== "\r") {
      return ch;
    }
  }
  return "";
}

function pseudoJsonToJson(json) {
  let ret = "";
  let at = 0, lasti = 0, lastch = "", hasMultipleParts = false;
  for (let i = 0, len = json.length; i < len; ++i) {
    let ch = json[i];
    if (/\s/.test(ch)) {
      continue;
    }

    if (ch === '"') {
      // Consume a string.
      ++i;
      while (i < len) {
        if (json[i] === "\\") {
          ++i;
        } else if (json[i] === '"') {
          break;
        }
        ++i;
      }
    } else if (ch === "'") {
      // Convert an invalid string into a valid one.
      ret += json.slice(at, i) + "\"";
      at = i + 1;
      ++i;

      while (i < len) {
        if (json[i] === "\\") {
          ++i;
        } else if (json[i] === "'") {
          break;
        }
        ++i;
      }

      if (i < len) {
        ret += json.slice(at, i) + "\"";
        at = i + 1;
      }
    } else if ((ch === "[" || ch === "{") &&
        (lastch === "]" || lastch === "}")) {
      // Multiple JSON messages in one... Make it into a single array by
      // inserting a comma and setting the "multiple parts" flag.
      ret += json.slice(at, i) + ",";
      hasMultipleParts = true;
      at = i;
    } else if (lastch === "," && (ch === "]" || ch === "}")) {
      // Trailing commas in arrays/objects.
      ret += json.slice(at, lasti);
      at = i;
    } else if (lastch === "/" && lasti === i - 1) {
      // Some kind of comment; remove it.
      if (ch === "/") {
        ret += json.slice(at, i - 1);
        at = i + json.slice(i).search(/\n|\r|$/);
        i = at - 1;
      } else if (ch === "*") {
        ret += json.slice(at, i - 1);
        at = json.indexOf("*/", i + 1) + 2;
        if (at === 1) {
          at = len;
        }
        i = at - 1;
      }
      ch = "\0";
    } else if (/[a-zA-Z$_]/.test(ch) && lastch !== ":") {
      // Non-quoted identifier. Quote it.
      ret += json.slice(at, i) + "\"";
      at = i;
      i = i + json.slice(i).search(/[^a-zA-Z0-9$_]|$/);
      ret += json.slice(at, i) + "\"";
      at = i;
    }

    lastch = ch;
    lasti = i;
  }

  ret += json.slice(at);
  if (hasMultipleParts) {
    ret = "[" + ret + "]";
  }

  return ret;
}

Json.isJSON = function (contentType, data) {
  // Workaround for JSON responses without proper content type
  // Let's consider all responses starting with "{" as JSON. In the worst
  // case there will be an exception when parsing. This means that no-JSON
  // responses (and post data) (with "{") can be parsed unnecessarily,
  // which represents a little overhead, but this happens only if the request
  // is actually expanded by the user in the UI (Net & Console panels).
  // Do a manual string search instead of checking (data.strip()[0] === "{")
  // to improve performance/memory usage.
  let len = data ? data.length : 0;
  for (let i = 0; i < len; i++) {
    let ch = data.charAt(i);
    if (ch === "{") {
      return true;
    }

    if (ch === " " || ch === "\t" || ch === "\n" || ch === "\r") {
      continue;
    }

    break;
  }

  if (!contentType) {
    return false;
  }

  contentType = contentType.split(";")[0];
  contentType = contentType.trim();
  return !!contentTypes[contentType];
};

// Exports from this module
module.exports = Json;

