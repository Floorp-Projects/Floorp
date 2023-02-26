/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function escapeRegExp(str) {
  const reRegExpChar = /[\\^$.*+?()[\]{}|]/g;
  return str.replace(reRegExpChar, "\\$&");
}

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

  return null;
}

export default function buildQuery(
  originalQuery,
  modifiers,
  { isGlobal = false, ignoreSpaces = false }
) {
  const { caseSensitive, regexMatch, wholeWord } = modifiers;

  if (originalQuery === "") {
    return new RegExp(originalQuery);
  }

  let query = originalQuery;

  // If we don't want to do a regexMatch, we need to escape all regex related characters
  // so they would actually match.
  if (!regexMatch) {
    query = escapeRegExp(query);
  }

  // ignoreWhiteSpace might return a negative lookbehind, and in such case, we want it
  // to be consumed as a RegExp part by the callsite, so this needs to be called after
  // the regexp is escaped.
  if (ignoreSpaces) {
    query = ignoreWhiteSpace(query);
  }

  query = wholeMatch(query, wholeWord);
  const flags = buildFlags(caseSensitive, isGlobal);

  if (flags) {
    return new RegExp(query, flags);
  }

  return new RegExp(query);
}
