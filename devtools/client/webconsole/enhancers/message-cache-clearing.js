/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MESSAGES_CLEAR,
} = require("devtools/client/webconsole/constants");

/**
 * This enhancer is responsible for clearing the messages caches using the
 * webconsoleClient when the user clear the messages (either by direct UI action, or via
 * `console.clear()`).
 */
function enableMessagesCacheClearing(hud) {
  return next => (reducer, initialState, enhancer) => {
    function messagesCacheClearingEnhancer(state, action) {
      state = reducer(state, action);

      const webConsoleClient = hud && hud.proxy ? hud.proxy.webConsoleClient : null;
      if (webConsoleClient && action.type === MESSAGES_CLEAR) {
        webConsoleClient.clearNetworkRequests();
        webConsoleClient.clearMessagesCache();
      }
      return state;
    }

    return next(messagesCacheClearingEnhancer, initialState, enhancer);
  };
}

module.exports = enableMessagesCacheClearing;
