/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PREF_PREFIX = "devtools.aboutdebugging.collapsibilities.";
const {
  DEBUG_TARGET_PANE,
} = require("devtools/client/aboutdebugging/src/constants");

/**
 * This module provides a collection of helper methods to read and update the debug
 * target pane's collapsibilities.
 */

/**
 * @return {Object}
 *         {
 *           key: constants.DEBUG_TARGET_PANE
 *           value: true - collapsed
 *                  false - expanded
 *         }
 */
function getDebugTargetCollapsibilities() {
  const map = new Map();

  for (const key of Object.values(DEBUG_TARGET_PANE)) {
    const pref = Services.prefs.getBoolPref(PREF_PREFIX + key, false);
    map.set(key, pref);
  }

  return map;
}
exports.getDebugTargetCollapsibilities = getDebugTargetCollapsibilities;

/**
 * @param collapsibilities - Same format to getDebugTargetCollapsibilities.
 */
function setDebugTargetCollapsibilities(collapsibilities) {
  for (const key of Object.values(DEBUG_TARGET_PANE)) {
    const isCollapsed = collapsibilities.get(key);
    Services.prefs.setBoolPref(PREF_PREFIX + key, isCollapsed);
  }
}
exports.setDebugTargetCollapsibilities = setDebugTargetCollapsibilities;
