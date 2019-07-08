/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Error logging middleware that will forward all actions that contain an error property
 * to the console.
 */
function errorLoggingMiddleware() {
  return next => action => {
    if (action.error) {
      const { error } = action;
      if (error.message) {
        console.error(`[ACTION FAILED] ${action.type}: ${error.message}`);
      } else if (typeof error === "string") {
        // All failure actions should dispatch an error object instead of a message.
        // We allow some flexibility to still provide some error logging.
        console.error(`[ACTION FAILED] ${action.type}: ${error}`);
        console.error(
          `[ACTION FAILED] ${action.type} should dispatch the error object!`
        );
      }

      if (error.stack) {
        console.error(error.stack);
      }
    }

    return next(action);
  };
}

module.exports = errorLoggingMiddleware;
