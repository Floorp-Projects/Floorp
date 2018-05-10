"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getDeviceFront = getDeviceFront;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getDeviceFront() {
  return {
    getDescription: function () {
      return {
        // Return anything that will not match Fennec v60
        apptype: "apptype",
        version: "version"
      };
    }
  };
}