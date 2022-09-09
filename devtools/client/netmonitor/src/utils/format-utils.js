/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

// Constants for formatting bytes.
const BYTES_IN_KB = 1000;
const BYTES_IN_MB = Math.pow(BYTES_IN_KB, 2);
const BYTES_IN_GB = Math.pow(BYTES_IN_KB, 3);
const MAX_BYTES_SIZE = 1000;
const MAX_KB_SIZE = 1000 * BYTES_IN_KB;
const MAX_MB_SIZE = 1000 * BYTES_IN_MB;

// Constants for formatting time.
const MAX_MILLISECOND = 1000;
const MAX_SECOND = 60 * MAX_MILLISECOND;

const REQUEST_DECIMALS = 2;

// Constants for formatting the priority, derived from nsISupportsPriority.idl
const PRIORITY_HIGH = -10;
const PRIORITY_NORMAL = 0;
const PRIORITY_LOW = 10;

function getSizeWithDecimals(size, decimals = REQUEST_DECIMALS) {
  return L10N.numberWithDecimals(size, decimals);
}

function getTimeWithDecimals(time) {
  return L10N.numberWithDecimals(time, REQUEST_DECIMALS);
}

function formatDecimals(size, decimals) {
  return size % 1 > 0 ? decimals : 0;
}

/**
 * Get a human-readable string from a number of bytes, with the B, kB, MB, or
 * GB value.
 */
function getFormattedSize(bytes, decimals = REQUEST_DECIMALS) {
  if (bytes < MAX_BYTES_SIZE) {
    return L10N.getFormatStr("networkMenu.sizeB", bytes);
  }
  if (bytes < MAX_KB_SIZE) {
    const kb = bytes / BYTES_IN_KB;
    const formattedDecimals = formatDecimals(kb, decimals);

    return L10N.getFormatStr(
      "networkMenu.size.kB",
      getSizeWithDecimals(kb, formattedDecimals)
    );
  }
  if (bytes < MAX_MB_SIZE) {
    const mb = bytes / BYTES_IN_MB;
    const formattedDecimals = formatDecimals(mb, decimals);
    return L10N.getFormatStr(
      "networkMenu.sizeMB",
      getSizeWithDecimals(mb, formattedDecimals)
    );
  }
  const gb = bytes / BYTES_IN_GB;
  const formattedDecimals = formatDecimals(gb, decimals);
  return L10N.getFormatStr(
    "networkMenu.sizeGB",
    getSizeWithDecimals(gb, formattedDecimals)
  );
}

/**
 * Get a human-readable string from a number of time, with the ms, s, or min
 * value.
 */
function getFormattedTime(ms) {
  if (ms < MAX_MILLISECOND) {
    return L10N.getFormatStr("networkMenu.millisecond", ms | 0);
  }
  if (ms < MAX_SECOND) {
    const sec = ms / MAX_MILLISECOND;
    return L10N.getFormatStr("networkMenu.second", getTimeWithDecimals(sec));
  }
  const min = ms / MAX_SECOND;
  return L10N.getFormatStr("networkMenu.minute", getTimeWithDecimals(min));
}

/**
 * Formats IP (v4 and v6) and port
 *
 * @param {string} ip - IP address
 * @param {string} port
 * @return {string} the formatted IP + port
 */
function getFormattedIPAndPort(ip, port) {
  if (!port) {
    return ip;
  }
  return ip.match(/:+/) ? `[${ip}]:${port}` : `${ip}:${port}`;
}

/**
 * Formats the priority of a request
 * Based on unix conventions
 * See xpcom/threads/nsISupportsPriority.idl
 *
 * @param {Number} priority - request priority
 */
function getRequestPriorityAsText(priority) {
  if (priority < PRIORITY_HIGH) {
    return "Highest";
  } else if (priority >= PRIORITY_HIGH && priority < PRIORITY_NORMAL) {
    return "High";
  } else if (priority === PRIORITY_NORMAL) {
    return "Normal";
  } else if (priority > PRIORITY_NORMAL && priority <= PRIORITY_LOW) {
    return "Low";
  }
  return "Lowest";
}

module.exports = {
  getFormattedIPAndPort,
  getFormattedSize,
  getFormattedTime,
  getSizeWithDecimals,
  getTimeWithDecimals,
  getRequestPriorityAsText,
};
