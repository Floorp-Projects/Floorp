/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This module was taken from narwhal:
//
// http://narwhaljs.org

var observers = [];

exports.when = function (observer) {
  observers.unshift(observer);
};

exports.send = function () {
  observers.forEach(function (observer) {
    observer();
  });
};
