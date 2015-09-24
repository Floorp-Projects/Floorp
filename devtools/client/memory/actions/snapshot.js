/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PROMISE } = require("devtools/client/shared/redux/middleware/promise");
const { actions } = require("../constants");

const takeSnapshot = exports.takeSnapshot = function takeSnapshot (front) {
  return {
    type: actions.TAKE_SNAPSHOT,
    [PROMISE]: front.saveHeapSnapshot()
  };
};
