/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum(
  [
    // Updates the selected node.
    "COMPATIBILITY_UPDATE_SELECTED_NODE_START",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE",

    // Updates the target browsers.
    "COMPATIBILITY_UPDATE_TARGET_BROWSERS",
  ],
  module.exports
);
