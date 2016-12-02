/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const {
  UPDATE_REQUESTS,
} = require("../constants");

const Requests = I.Record({
  items: [],
});

function updateRequests(state, action) {
  return state.set("items", action.items || state.items);
}

function requests(state = new Requests(), action) {
  switch (action.type) {
    case UPDATE_REQUESTS:
      return updateRequests(state, action);
    default:
      return state;
  }
}

module.exports = requests;
