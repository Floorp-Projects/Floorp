/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getOngoingSearch(state) {
  return state.search.ongoingSearch;
}

function getSearchStatus(state) {
  return state.search.status;
}

module.exports = {
  getOngoingSearch,
  getSearchStatus,
};
