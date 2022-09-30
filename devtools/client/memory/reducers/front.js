/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { actions } = require("resource://devtools/client/memory/constants.js");

module.exports = (front = null, action) => {
  return action.type === actions.UPDATE_MEMORY_FRONT ? action.front : front;
};
