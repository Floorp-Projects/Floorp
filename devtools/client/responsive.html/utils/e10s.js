/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The prefix used for RDM messages in content.
// see: devtools/client/responsivedesign/responsivedesign-child.js
const MESSAGE_PREFIX = "ResponsiveMode:";
const REQUEST_DONE_SUFFIX = ":Done";

/**
 * Registers a message `listener` that is called every time messages of
 * specified `message` is emitted on the given message manager.
 * @param {nsIMessageListenerManager} mm
 *    The Message Manager
 * @param {String} message
 *    The message. It will be prefixed with the constant `MESSAGE_PREFIX`
 * @param {Function} listener
 *    The listener function that processes the message.
 */
function on(mm, message, listener) {
  mm.addMessageListener(MESSAGE_PREFIX + message, listener);
}
exports.on = on;

/**
 * Removes a message `listener` for the specified `message` on the given
 * message manager.
 * @param {nsIMessageListenerManager} mm
 *    The Message Manager
 * @param {String} message
 *    The message. It will be prefixed with the constant `MESSAGE_PREFIX`
 * @param {Function} listener
 *    The listener function that processes the message.
 */
function off(mm, message, listener) {
  mm.removeMessageListener(MESSAGE_PREFIX + message, listener);
}
exports.off = off;

/**
 * Resolves a promise the next time the specified `message` is sent over the
 * given message manager.
 * @param {nsIMessageListenerManager} mm
 *    The Message Manager
 * @param {String} message
 *    The message. It will be prefixed with the constant `MESSAGE_PREFIX`
 * @returns {Promise}
 *    A promise that is resolved when the given message is emitted.
 */
function once(mm, message) {
  return new Promise(resolve => {
    on(mm, message, function onMessage({data}) {
      off(mm, message, onMessage);
      resolve(data);
    });
  });
}
exports.once = once;

/**
 * Asynchronously emit a `message` to the listeners of the given message
 * manager.
 *
 * @param {nsIMessageListenerManager} mm
 *    The Message Manager
 * @param {String} message
 *    The message. It will be prefixed with the constant `MESSAGE_PREFIX`.
 * @param {Object} data
 *    A JSON object containing data to be delivered to the listeners.
 */
function emit(mm, message, data) {
  mm.sendAsyncMessage(MESSAGE_PREFIX + message, data);
}
exports.emit = emit;

/**
 * Asynchronously send a "request" over the given message manager, and returns
 * a promise that is resolved when the request is complete.
 *
 * @param {nsIMessageListenerManager} mm
 *    The Message Manager
 * @param {String} message
 *    The message. It will be prefixed with the constant `MESSAGE_PREFIX`, and
 *    also suffixed with `REQUEST_DONE_SUFFIX` for the reply.
 * @param {Object} data
 *    A JSON object containing data to be delivered to the listeners.
 * @returns {Promise}
 *    A promise that is resolved when the request is done.
 */
function request(mm, message, data) {
  let done = once(mm, message + REQUEST_DONE_SUFFIX);

  emit(mm, message, data);

  return done;
}
exports.request = request;
