/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helpers for async functions. Async functions are generator functions that are
 * run by Tasks. An async function returns a Promise for the resolution of the
 * function. When the function returns, the promise is resolved with the
 * returned value. If it throws the promise rejects with the thrown error.
 *
 * See Task documentation at https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Task.jsm.
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
    let onEvent = function(ev) {
      element.removeEventListener(event, onEvent, useCapture);
      resolve(ev);
    };
    element.addEventListener(event, onEvent, useCapture);
  });
};
