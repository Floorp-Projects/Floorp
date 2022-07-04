/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { MESSAGES_CLEAR } = require("devtools/client/webconsole/constants");

/**
 * This enhancer is responsible for clearing the messages caches using the
 * webconsoleFront when the user clear the messages (either by direct UI action, or via
 * `console.clear()`).
 */
function enableMessagesCacheClearing(webConsoleUI) {
  return next => (reducer, initialState, enhancer) => {
    function messagesCacheClearingEnhancer(state, action) {
      const storeHadMessages =
        state?.messages?.mutableMessagesById &&
        state.messages.mutableMessagesById.size > 0;
      state = reducer(state, action);

      if (storeHadMessages && webConsoleUI && action.type === MESSAGES_CLEAR) {
        webConsoleUI.clearMessagesCache();

        // cleans up all the network data provider internal state
        webConsoleUI.networkDataProvider?.destroy();

        if (webConsoleUI.hud?.toolbox) {
          webConsoleUI.hud.toolbox.setErrorCount(0);
        }
      }
      return state;
    }

    return next(messagesCacheClearingEnhancer, initialState, enhancer);
  };
}

module.exports = enableMessagesCacheClearing;
