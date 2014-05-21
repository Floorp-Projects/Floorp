/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Disclaimer: Some of the functions in this module implement APIs from
// Jeremy Ashkenas's http://underscorejs.org/ library and all credits for
// those goes to him.

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { arity, name, derive, invoke } = require("./helpers");
const { setTimeout, clearTimeout, setImmediate } = require("../../timers");

/**
 * Takes a function and returns a wrapped one instead, calling which will call
 * original function in the next turn of event loop. This is basically utility
 * to do `setImmediate(function() { ... })`, with a difference that returned
 * function is reused, instead of creating a new one each time. This also allows
 * to use this functions as event listeners.
 */
const defer = f => derive(function(...args) {
  setImmediate(invoke, f, args, this);
}, f);
exports.defer = defer;
// Exporting `remit` alias as `defer` may conflict with promises.
exports.remit = defer;

/**
 * Much like setTimeout, invokes function after wait milliseconds. If you pass
 * the optional arguments, they will be forwarded on to the function when it is
 * invoked.
 */
const delay = function delay(f, ms, ...args) {
  setTimeout(() => f.apply(this, args), ms);
};
exports.delay = delay;

/**
 * From underscore's `_.debounce`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 */
const debounce = function debounce (fn, wait) {
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
exports.debounce = debounce;

/**
 * From underscore's `_.throttle`
 * http://underscorejs.org
 * (c) 2009-2014 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
 * Underscore may be freely distributed under the MIT license.
 */
const throttle = function throttle (func, wait, options) {
  let context, args, result;
  let timeout = null;
  let previous = 0;
  options || (options = {});
  let later = function() {
    previous = options.leading === false ? 0 : Date.now();
    timeout = null;
    result = func.apply(context, args);
    context = args = null;
  };
  return function() {
    let now = Date.now();
    if (!previous && options.leading === false) previous = now;
    let remaining = wait - (now - previous);
    context = this;
    args = arguments;
    if (remaining <= 0) {
      clearTimeout(timeout);
      timeout = null;
      previous = now;
      result = func.apply(context, args);
      context = args = null;
    } else if (!timeout && options.trailing !== false) {
      timeout = setTimeout(later, remaining);
    }
    return result;
  };
};
exports.throttle = throttle;
