/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("resource://devtools/client/shared/enum.js");

createEnum(
  [
    // Append node and their children on DOM mutation
    "COMPATIBILITY_APPEND_NODE_START",
    "COMPATIBILITY_APPEND_NODE_SUCCESS",
    "COMPATIBILITY_APPEND_NODE_FAILURE",
    "COMPATIBILITY_APPEND_NODE_COMPLETE",

    // Remove references to node that is removed
    // programmatically whose fronts are destroyed.
    "COMPATIBILITY_CLEAR_DESTROYED_NODES",

    // Init user settings.
    "COMPATIBILITY_INIT_USER_SETTINGS_START",
    "COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS",
    "COMPATIBILITY_INIT_USER_SETTINGS_FAILURE",
    "COMPATIBILITY_INIT_USER_SETTINGS_COMPLETE",

    // Append node using internal helper that caused issues.
    "COMPATIBILITY_INTERNAL_APPEND_NODE",

    // Updates a node via the internal helper
    "COMPATIBILITY_INTERNAL_NODE_UPDATE",

    // Remove references to node that is removed
    // in Markup Inspector but retained by DevTools
    // using the internal helper.
    "COMPATIBILITY_INTERNAL_REMOVE_NODE",

    // Updates the selected node issues using internal helper.
    "COMPATIBILITY_INTERNAL_UPDATE_SELECTED_NODE_ISSUES",

    // Clean up removed node from node list
    "COMPATIBILITY_REMOVE_NODE_START",
    "COMPATIBILITY_REMOVE_NODE_SUCCESS",
    "COMPATIBILITY_REMOVE_NODE_FAILURE",
    "COMPATIBILITY_REMOVE_NODE_COMPLETE",

    // Update node on attribute mutation
    "COMPATIBILITY_UPDATE_NODE_START",
    "COMPATIBILITY_UPDATE_NODE_SUCCESS",
    "COMPATIBILITY_UPDATE_NODE_FAILURE",
    "COMPATIBILITY_UPDATE_NODE_COMPLETE",

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
