/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { CHANGE_LOCATION } = require("../actions/index");

const INITIAL_LOCATION = "about:blank";

const reducers = {

  [CHANGE_LOCATION](_, action) {
    return action.location;
  },

};

module.exports = function(location = INITIAL_LOCATION, action) {
  const reducer = reducers[action.type];
  if (!reducer) {
    return location;
  }
  return reducer(location, action);
};
