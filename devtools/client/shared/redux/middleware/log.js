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
      console.log("[DISPATCH]", JSON.stringify(action, null, 2));
    } catch (e) {
      console.log("[DISPATCH]", action);
    }
    next(action);
  };
}

exports.log = log;
