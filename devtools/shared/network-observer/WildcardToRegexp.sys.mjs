/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Converts a URL-like string which might include the `*` character as a wildcard
 * to a regular expression. They are used to match against actual URLs for the
 * request blocking feature from DevTools.
 *
 * The returned regular expression is case insensitive.
 *
 * @param {string} url
 *     A URL-like string which can contain one or several `*` as wildcard
 *     characters.
 * @return {RegExp}
 *     A regular expression which can be used to match URLs compatible with the
 *     provided url "template".
 */
export function wildcardToRegExp(url) {
  return new RegExp(url.split("*").map(regExpEscape).join(".*"), "i");
}

/**
 * Escapes all special RegExp characters in the given string.
 */
const regExpEscape = s => {
  return s.replace(/[|\\{}()[\]^$+*?.]/g, "\\$&");
};
