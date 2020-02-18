/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Returns first 1024 characters of value for use as a tooltip.
 * @param object
 * @returns {*}
 */
function limitTooltipLength(object) {
  return object.length > 1024 ? object.substring(0, 1024) + "…" : object;
}

module.exports = {
  limitTooltipLength,
};
