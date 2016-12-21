/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions } = require("../constants");

module.exports = function (filterString = null, action) {
  if (action.type === actions.SET_FILTER_STRING) {
    return action.filter || null;
  }

  return filterString;
};
