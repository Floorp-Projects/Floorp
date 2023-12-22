/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Based on the Natural Sort algorithm for Javascript - Version 0.8.1 - adapted
 * for Firefox DevTools and released under the MIT license.
 *
 * Author: Jim Palmer (based on chunking idea from Dave Koelle)
 *
 * Repository:
 *   https://github.com/overset/javascript-natural-sort/
 */

"use strict";

const tokenizeNumbersRx =
  /(^([+\-]?\d+(?:\.\d*)?(?:[eE][+\-]?\d+)?(?=\D|\s|$))|^0x[\da-fA-F]+$|\d+)/g;
const hexRx = /^0x[0-9a-f]+$/i;
const startsWithNullRx = /^\0/;
const endsWithNullRx = /\0$/;
const whitespaceRx = /\s+/g;
const startsWithZeroRx = /^0/;
const versionRx = /^([\w-]+-)?\d+\.\d+\.\d+$/;

/**
 * Sort numbers, strings, IP Addresses, Dates, Filenames, version numbers etc.
 * "the way humans do."
 *
 * @param  {Object} a
 *         Passed in by Array.sort(a, b)
 * @param  {Object} b
 *         Passed in by Array.sort(a, b)
 * @param  {String} sessionString
 *         Client-side value of storage-expires-session l10n string.
 *         Since this function can be called from both the client and the server,
 *         and given that client and server might have different locale, we can't compute
 *         the localized string directly from here.
 * @param  {Boolean} insensitive
 *         Should the search be case insensitive?
 */
// eslint-disable-next-line complexity
function naturalSort(a = "", b = "", sessionString, insensitive = false) {
  // Ensure we are working with trimmed strings
  a = (a + "").trim();
  b = (b + "").trim();

  if (insensitive) {
    a = a.toLowerCase();
    b = b.toLowerCase();
    sessionString = sessionString.toLowerCase();
  }

  // Chunk/tokenize - Here we split the strings into arrays or strings and
  // numbers.
  const aChunks = a
    .replace(tokenizeNumbersRx, "\0$1\0")
    .replace(startsWithNullRx, "")
    .replace(endsWithNullRx, "")
    .split("\0");
  const bChunks = b
    .replace(tokenizeNumbersRx, "\0$1\0")
    .replace(startsWithNullRx, "")
    .replace(endsWithNullRx, "")
    .split("\0");

  // Hex or date detection.
  const aHexOrDate =
    parseInt(a.match(hexRx), 16) || (!versionRx.test(a) && Date.parse(a));
  const bHexOrDate =
    parseInt(b.match(hexRx), 16) || (!versionRx.test(b) && Date.parse(b));

  if (
    (aHexOrDate || bHexOrDate) &&
    (a === sessionString || b === sessionString)
  ) {
    // We have a date and a session string. Move "Session" above the date
    // (for session cookies)
    if (a === sessionString) {
      return -1;
    } else if (b === sessionString) {
      return 1;
    }
  }

  // Try and sort Hex codes or Dates.
  if (aHexOrDate && bHexOrDate) {
    if (aHexOrDate < bHexOrDate) {
      return -1;
    } else if (aHexOrDate > bHexOrDate) {
      return 1;
    }
    return 0;
  }

  // Natural sorting through split numeric strings and default strings
  const aChunksLength = aChunks.length;
  const bChunksLength = bChunks.length;
  const maxLen = Math.max(aChunksLength, bChunksLength);

  for (let i = 0; i < maxLen; i++) {
    const aChunk = normalizeChunk(aChunks[i] || "", aChunksLength);
    const bChunk = normalizeChunk(bChunks[i] || "", bChunksLength);

    // Handle numeric vs string comparison - number < string
    if (isNaN(aChunk) !== isNaN(bChunk)) {
      return isNaN(aChunk) ? 1 : -1;
    }

    // If unicode use locale comparison
    // eslint-disable-next-line no-control-regex
    if (/[^\x00-\x80]/.test(aChunk + bChunk) && aChunk.localeCompare) {
      const comp = aChunk.localeCompare(bChunk);
      return comp / Math.abs(comp);
    }
    if (aChunk < bChunk) {
      return -1;
    } else if (aChunk > bChunk) {
      return 1;
    }
  }
  return null;
}

// Normalize spaces; find floats not starting with '0', string or 0 if not
// defined
const normalizeChunk = function (str, length) {
  return (
    ((!str.match(startsWithZeroRx) || length == 1) && parseFloat(str)) ||
    str.replace(whitespaceRx, " ").trim() ||
    0
  );
};

exports.naturalSortCaseSensitive = function naturalSortCaseSensitive(
  a,
  b,
  sessionString
) {
  return naturalSort(a, b, sessionString, false);
};

exports.naturalSortCaseInsensitive = function naturalSortCaseInsensitive(
  a,
  b,
  sessionString
) {
  return naturalSort(a, b, sessionString, true);
};
