/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const s4 = function () {
  return Math.floor((1 + Math.random()) * 0x10000)
             .toString(16)
             .substring(1);
};

let uuid = function () {
  return "ss-s-s-s-sss".replace(/s/g, function () {
    return s4();
  });
};

module.exports = { uuid };
