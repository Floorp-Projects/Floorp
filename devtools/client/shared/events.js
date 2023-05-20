/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Prevent event default behaviour and stop its propagation.
 * @param  {Object} event
 *         Event or react synthetic event.
 */
exports.preventDefaultAndStopPropagation = function (event) {
  event.preventDefault();
  event.stopPropagation();
  if (event.nativeEvent) {
    if (event.nativeEvent.preventDefault) {
      event.nativeEvent.preventDefault();
    }
    if (event.nativeEvent.stopPropagation) {
      event.nativeEvent.stopPropagation();
    }
  }
};
