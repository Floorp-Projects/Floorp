/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * From underscore's `_.debounce`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 *
 * [and in turn extracted from "sdk/lang/functional/concurrent.js"]
 */
exports.debounce = function (fn, wait) {
  let timeout, args, context, timestamp, result;

  let later = function () {
    let last = Date.now() - timestamp;
    if (last < wait) {
      timeout = setTimeout(later, wait - last);
    } else {
      timeout = null;
      result = fn.apply(context, args);
      context = args = null;
    }
  };

  return function (...aArgs) {
    context = this;
    args = aArgs;
    timestamp  = Date.now();
    if (!timeout) {
      timeout = setTimeout(later, wait);
    }

    return result;
  };
};
