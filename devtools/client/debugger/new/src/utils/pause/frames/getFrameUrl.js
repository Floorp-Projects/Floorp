"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFrameUrl = getFrameUrl;

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getFrameUrl(frame) {
  return (0, _lodash.get)(frame, "source.url", "") || "";
}