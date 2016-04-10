/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

importScripts("util.js");
importScripts("test-vectors.js");

var window = this;

function finish(result) {
  postMessage(result);
}

function complete(test, valid) {
  return function(x) {
    if (valid) {
      finish(valid(x));
    } else {
      finish(true);
    }
  };
}

function memcmp_complete(test, value) {
  return function (x) {
    finish(util.memcmp(x, value));
  };
}

function error(test) {
  return function (x) {
    throw x;
  };
}

onmessage = function (msg) {
  var test = eval("(" + msg.data + ")");

  try {
    test.call({complete: finish});
  } catch (err) {
    error(`Failed to run worker test: ${err}\n`);
  }
};
