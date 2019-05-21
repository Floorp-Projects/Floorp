/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Create a debouncing function wrapper to only call the target function after a certain
 * amount of time has passed without it being called.
 *
 * @param {Function} func
 *         The function to debounce
 * @param {number} wait
 *         The wait period
 * @param {Object} scope
 *         The scope to use for func
 * @param {Boolean} immediate
 *         Whether to execute the method immediately on the first call.
 *         Optional. Default `false`.
 * @return {Function} The debounced function
 */
exports.debounce = function(func, wait, scope, immediate = false) {
  let timer = null;

  return function() {
    if (timer) {
      clearTimeout(timer);
    }

    const args = arguments;
    timer = setTimeout(function() {
      timer = null;
      func.apply(scope, args);
    }, wait);

    if (immediate) {
      immediate = false;
      func.apply(scope, args);
    }
  };
};
