/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helpers for async functions. An async function returns a Promise for the
 * resolution of the function. When the function returns, the promise is
 * resolved with the returned value. If it throws the promise rejects with
 * the thrown error.
 */

/**
 * Adds an event listener to the given element, and then removes its event
 * listener once the event is called, returning the event object as a promise.
 * @param  Element element
 *         The DOM element to listen on
 * @param  String event
 *         The name of the event type to listen for
 * @param  Boolean useCapture
 *         Should we initiate the capture phase?
 * @return Promise
 *         The promise resolved with the event object when the event first
 *         happens
 */
exports.listenOnce = function listenOnce(element, event, useCapture) {
  return new Promise(function(resolve, reject) {
    const onEvent = function(ev) {
      element.removeEventListener(event, onEvent, useCapture);
      resolve(ev);
    };
    element.addEventListener(event, onEvent, useCapture);
  });
};

// Return value when `safeAsyncMethod` catches an error.
const SWALLOWED_RET = Symbol("swallowed");

/**
 * Wraps the provided async method in a try/catch block.
 * If an error is caught while running the method, check the provided condition
 * to decide whether the error should bubble or not.
 *
 * @param  {Function} asyncFn
 *         The async method to wrap.
 * @param  {Function} shouldSwallow
 *         Function that will run when an error is caught. If it returns true,
 *         the error will be swallowed. Otherwise, it will bubble up.
 * @return {Function} The wrapped method.
 */
exports.safeAsyncMethod = function(asyncFn, shouldSwallow) {
  return async function(...args) {
    try {
      const ret = await asyncFn(...args);
      return ret;
    } catch (e) {
      if (shouldSwallow()) {
        console.warn("Async method failed in safeAsyncMethod", e);
        return SWALLOWED_RET;
      }
      throw e;
    }
  };
};
