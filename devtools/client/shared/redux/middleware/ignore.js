/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const IGNORING = Symbol("IGNORING");
const START_IGNORE_ACTION = "START_IGNORE_ACTION";

/**
 * A middleware that prevents any action of being called once it is activated.
 * This is useful to apply while destroying a given panel, as it will ignore all calls
 * to actions, where we usually make our client -> server communications.
 * This middleware should be declared before any other middleware to  to effectively
 * ignore every actions.
 */
function ignore({ getState }) {
  return next => action => {
    if (action.type === START_IGNORE_ACTION) {
      getState()[IGNORING] = true;
      return null;
    }

    if (getState()[IGNORING]) {
      // We only print the action type, and not the whole action object, as it can holds
      // very complex data that would clutter stdout logs and make them impossible
      // to parse for treeherder.
      console.warn("IGNORED REDUX ACTION:", action.type);
      return null;
    }

    return next(action);
  };
}

module.exports = {
  ignore,
  START_IGNORE_ACTION: { type: START_IGNORE_ACTION },
};
