/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const REQUEST_DONE_SUFFIX = ":done";

function wait(win, type) {
  return new Promise(resolve => {
    let onMessage = event => {
      if (event.data.type !== type) {
        return;
      }
      win.removeEventListener("message", onMessage);
      resolve();
    };
    win.addEventListener("message", onMessage);
  });
}

/**
 * Post a message to some window.
 *
 * @param win
 *        The window to post to.
 * @param typeOrMessage
 *        Either a string or and an object representing the message to send.
 *        If this is a string, it will be expanded into an object with the string as the
 *        `type` field.  If this is an object, it will be sent as is.
 */
function post(win, typeOrMessage) {
  // When running unit tests on XPCShell, there is no window to send messages to.
  if (!win) {
    return;
  }

  let message = typeOrMessage;
  if (typeof typeOrMessage == "string") {
    message = {
      type: typeOrMessage,
    };
  }
  win.postMessage(message, "*");
}

function request(win, type) {
  let done = wait(win, type + REQUEST_DONE_SUFFIX);
  post(win, type);
  return done;
}

exports.wait = wait;
exports.post = post;
exports.request = request;
