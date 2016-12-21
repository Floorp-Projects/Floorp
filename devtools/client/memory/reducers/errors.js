/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ERROR_TYPE: TASK_ERROR_TYPE } = require("devtools/client/shared/redux/middleware/task");

/**
 * Handle errors dispatched from task middleware and
 * store them so we can check in tests or dump them out.
 */
module.exports = function (state = [], action) {
  switch (action.type) {
    case TASK_ERROR_TYPE:
      return [...state, action.error];
  }
  return state;
};
