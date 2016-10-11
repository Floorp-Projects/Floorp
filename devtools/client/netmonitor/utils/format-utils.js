/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("../l10n");

// Constants for formatting bytes.
const BYTES_IN_KB = 1024;
const BYTES_IN_MB = Math.pow(BYTES_IN_KB, 2);
const BYTES_IN_GB = Math.pow(BYTES_IN_KB, 3);
const MAX_BYTES_SIZE = 1000;
const MAX_KB_SIZE = 1000 * BYTES_IN_KB;
const MAX_MB_SIZE = 1000 * BYTES_IN_MB;

const CONTENT_SIZE_DECIMALS = 2;

/**
 * Get a human-readable string from a number of bytes, with the B, KB, MB, or
 * GB value. Note that the transition between abbreviations is by 1000 rather
 * than 1024 in order to keep the displayed digits smaller as "1016 KB" is
 * more awkward than 0.99 MB"
 */
function getFormattedSize(bytes) {
  if (bytes < MAX_BYTES_SIZE) {
    return L10N.getFormatStr("networkMenu.sizeB", bytes);
  } else if (bytes < MAX_KB_SIZE) {
    let kb = bytes / BYTES_IN_KB;
    let size = L10N.numberWithDecimals(kb, CONTENT_SIZE_DECIMALS);
    return L10N.getFormatStr("networkMenu.sizeKB", size);
  } else if (bytes < MAX_MB_SIZE) {
    let mb = bytes / BYTES_IN_MB;
    let size = L10N.numberWithDecimals(mb, CONTENT_SIZE_DECIMALS);
    return L10N.getFormatStr("networkMenu.sizeMB", size);
  }
  let gb = bytes / BYTES_IN_GB;
  let size = L10N.numberWithDecimals(gb, CONTENT_SIZE_DECIMALS);
  return L10N.getFormatStr("networkMenu.sizeGB", size);
}

module.exports = {
  getFormattedSize
};
