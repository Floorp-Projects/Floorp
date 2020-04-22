/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum(
  [
    // Append node that caused issues.
    "COMPATIBILITY_APPEND_NODE",

    // Init user settings.
    "COMPATIBILITY_INIT_USER_SETTINGS_START",
    "COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS",
    "COMPATIBILITY_INIT_USER_SETTINGS_FAILURE",
    "COMPATIBILITY_INIT_USER_SETTINGS_COMPLETE",

    // Updates a node.
    "COMPATIBILITY_UPDATE_NODE",

    // Updates nodes.
    "COMPATIBILITY_UPDATE_NODES_START",
    "COMPATIBILITY_UPDATE_NODES_SUCCESS",
    "COMPATIBILITY_UPDATE_NODES_FAILURE",
    "COMPATIBILITY_UPDATE_NODES_COMPLETE",

    // Updates the selected node.
    "COMPATIBILITY_UPDATE_SELECTED_NODE_START",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE",
    "COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE",

    // Updates the selected node issues.
    "COMPATIBILITY_UPDATE_SELECTED_NODE_ISSUES",

    // Updates the settings panel visibility.
    "COMPATIBILITY_UPDATE_SETTINGS_VISIBILITY",

    // Updates the target browsers.
    "COMPATIBILITY_UPDATE_TARGET_BROWSERS_START",
    "COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS",
    "COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE",
    "COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE",

    // Updates the top level target.
    "COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START",
    "COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS",
    "COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE",
    "COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE",
  ],
  module.exports
);
