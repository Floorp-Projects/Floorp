/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "stable"
};

const { search, remove, store } = require("./passwords/utils");
const { defer, delay } = require("./lang/functional");

/**
 * Utility function that returns `onComplete` and `onError` callbacks form the
 * given `options` objects. Also properties are removed from the passed
 * `options` objects.
 * @param {Object} options
 *    Object that is passed to the exported functions of this module.
 * @returns {Function[]}
 *    Array with two elements `onComplete` and `onError` functions.
 */
function getCallbacks(options) {
  let value = [
    'onComplete' in options ? options.onComplete : null,
    'onError' in options ? defer(options.onError) : console.exception
  ];

  delete options.onComplete;
  delete options.onError;

  return value;
};

/**
 * Creates a wrapper function that tries to call `onComplete` with a return
 * value of the wrapped function or falls back to `onError` if wrapped function
 * throws an exception.
 */
function createWrapperMethod(wrapped) {
  return function (options) {
    let [ onComplete, onError ] = getCallbacks(options);
    try {
      let value = wrapped(options);
      if (onComplete) {
        delay(function() {
          try {
            onComplete(value);
          } catch (exception) {
            onError(exception);
          }
        });
      }
    } catch (exception) {
      onError(exception);
    }
  };
}

exports.search = createWrapperMethod(search);
exports.store = createWrapperMethod(store);
exports.remove = createWrapperMethod(remove);
