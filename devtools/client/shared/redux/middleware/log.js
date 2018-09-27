/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * A middleware that logs all actions coming through the system
 * to the console.
 */
function log({ dispatch, getState }) {
  return next => action => {
    try {
      // whitelist of fields, rather than printing the whole object
      console.log("[DISPATCH] action type:", action.type);
      /*
       * USE WITH CAUTION!! This will output everything from an action object,
       * and these can be quite large. Printing out large objects will slow
       * down tests and cause test failures
       *
       * console.log("[DISPATCH]", JSON.stringify(action, null, 2));
       */
    } catch (e) {
      // this occurs if JSON.stringify throws.
      console.warn(e);
      console.log("[DISPATCH]", action);
    }
    next(action);
  };
}

exports.log = log;
