/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

/**
 * A middleware that stores every action coming through the store in the passed
 * in logging object. Should only be used for tests, as it collects all
 * action information, which will cause memory bloat.
 */
exports.history = (log=[]) => ({ dispatch, getState }) => {
  if (!DevToolsUtils.testing) {
    console.warn(`Using history middleware stores all actions in state for testing\
                  and devtools is not currently running in test mode. Be sure this is\
                  intentional.`);
  }
  return next => action => {
    log.push(action);
    next(action);
  };
};
