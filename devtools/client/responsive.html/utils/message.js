/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");

const REQUEST_DONE_SUFFIX = ":done";

function wait(win, type) {
  let deferred = promise.defer();

  let onMessage = event => {
    if (event.data.type !== type) {
      return;
    }
    win.removeEventListener("message", onMessage);
    deferred.resolve();
  };
  win.addEventListener("message", onMessage);

  return deferred.promise;
}

function post(win, type) {
  win.postMessage({ type }, "*");
}

function request(win, type) {
  let done = wait(win, type + REQUEST_DONE_SUFFIX);
  post(win, type);
  return done;
}

exports.wait = wait;
exports.post = post;
exports.request = request;
