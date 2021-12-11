/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/dom/content/constants");

/**
 * Initial state definition
 */
function getInitialState() {
  return "";
}

/**
 * Filter displayed object properties.
 */
function filter(state = getInitialState(), action) {
  if (action.type == constants.SET_VISIBILITY_FILTER) {
    return action.filter;
  }

  return state;
}

// Exports from this module
exports.filter = filter;
