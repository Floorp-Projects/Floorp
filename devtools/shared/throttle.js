/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * From underscore's `_.throttle`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 *
 * Returns a function, that, when invoked, will only be triggered at most once during a
 * given window of time. The throttled function will run as much as it can, without ever
 * going more than once per wait duration.
 *
 * @param  {Function} func
 *         The function to throttle
 * @param  {number} wait
 *         The wait period
 * @param  {Object} scope
 *         The scope to use for func
 * @return {Function} The throttled function
 */
function throttle(func, wait, scope) {
  let args, result;
  let timeout = null;
  let previous = 0;

  const later = function() {
    previous = Date.now();
    timeout = null;
    result = func.apply(scope, args);
    args = null;
  };

  return function() {
    const now = Date.now();
    const remaining = wait - (now - previous);
    args = arguments;
    if (remaining <= 0) {
      clearTimeout(timeout);
      timeout = null;
      previous = now;
      result = func.apply(scope, args);
      args = null;
    } else if (!timeout) {
      timeout = setTimeout(later, remaining);
    }
    return result;
  };
}

exports.throttle = throttle;
