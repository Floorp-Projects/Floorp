/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_SELECTED_PAGE,
} = require("devtools/client/application/src/constants");

function updateSelectedPage(selectedPage) {
  return {
    type: UPDATE_SELECTED_PAGE,
    selectedPage,
  };
}

module.exports = {
  updateSelectedPage,
};
