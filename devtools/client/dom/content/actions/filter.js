/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require("devtools/client/dom/content/constants");

/**
 * Used to filter DOM panel content.
 */
function setVisibilityFilter(filter) {
  return {
    filter,
    type: constants.SET_VISIBILITY_FILTER,
  };
}

// Exports from this module
exports.setVisibilityFilter = setVisibilityFilter;
