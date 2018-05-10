"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = buildQuery;

var _escapeRegExp = require("devtools/client/shared/vendor/lodash").escapeRegExp;

var _escapeRegExp2 = _interopRequireDefault(_escapeRegExp);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Ignore doing outline matches for less than 3 whitespaces
 *
 * @memberof utils/source-search
 * @static
 */
function ignoreWhiteSpace(str) {
  return /^\s{0,2}$/.test(str) ? "(?!\\s*.*)" : str;
}

function wholeMatch(query, wholeWord) {
  if (query === "" || !wholeWord) {
    return query;
  }

  return `\\b${query}\\b`;
}

function buildFlags(caseSensitive, isGlobal) {
  if (caseSensitive && isGlobal) {
    return "g";
  }

  if (!caseSensitive && isGlobal) {
    return "gi";
  }

  if (!caseSensitive && !isGlobal) {
    return "i";
  }

  return;
}

function buildQuery(originalQuery, modifiers, {
  isGlobal = false,
  ignoreSpaces = false
}) {
  const {
    caseSensitive,
    regexMatch,
    wholeWord
  } = modifiers;

  if (originalQuery === "") {
    return new RegExp(originalQuery);
  }

  let query = originalQuery;

  if (ignoreSpaces) {
    query = ignoreWhiteSpace(query);
  }

  if (!regexMatch) {
    query = (0, _escapeRegExp2.default)(query);
  }

  query = wholeMatch(query, wholeWord);
  const flags = buildFlags(caseSensitive, isGlobal);

  if (flags) {
    return new RegExp(query, flags);
  }

  return new RegExp(query);
}