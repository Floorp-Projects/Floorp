/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Truncate the string and add ellipsis to the middle of the string.
 */
function truncateString(str, maxLength) {
  if (!str || str.length <= maxLength) {
    return str;
  }

  return (
    str.substring(0, Math.ceil(maxLength / 2)) +
    "…" +
    str.substring(str.length - Math.floor(maxLength / 2))
  );
}

exports.truncateString = truncateString;
