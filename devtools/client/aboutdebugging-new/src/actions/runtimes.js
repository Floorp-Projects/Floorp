/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_SELECTED_RUNTIME,
} = require("../constants");

function updateSelectedRuntime(runtime) {
  return {
    type: UPDATE_SELECTED_RUNTIME,
    runtime,
  };
}

module.exports = {
  updateSelectedRuntime,
};
