/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAllNetworkMessagesUpdateById,
} = require("devtools/client/webconsole/selectors/messages");

const {
  getMessage,
  getAllMessagesUiById,
} = require("devtools/client/webconsole/selectors/messages");

const {
  MESSAGE_OPEN,
  NETWORK_MESSAGE_UPDATE,
} = require("devtools/client/webconsole/constants");

/**
 * This enhancer is responsible for fetching HTTP details data
 * collected by the backend. The fetch happens on-demand
 * when the user expands network log in order to inspect it.
 *
 * This way we don't slow down the Console logging by fetching.
 * unnecessary data over RDP.
 */
function enableNetProvider(webConsoleUI) {
  return next => (reducer, initialState, enhancer) => {
    function netProviderEnhancer(state, action) {
      const dataProvider =
        webConsoleUI &&
        webConsoleUI.wrapper &&
        webConsoleUI.wrapper.networkDataProvider;

      if (!dataProvider) {
        return reducer(state, action);
      }

      const { type } = action;
      const newState = reducer(state, action);

      // If network message has been opened, fetch all HTTP details
      // from the backend. It can happen (especially in test) that
      // the message is opened before all network event updates are
      // received. The rest of updates will be handled below, see:
      // NETWORK_MESSAGE_UPDATE action handler.
      if (type == MESSAGE_OPEN) {
        const updates = getAllNetworkMessagesUpdateById(newState);
        const message = updates[action.id];
        if (message && !message.openedOnce && message.source == "network") {
          dataProvider.onNetworkEvent(message);
          message.updates.forEach(updateType => {
            dataProvider.onNetworkEventUpdate({
              packet: { updateType: updateType },
              networkInfo: message,
            });
          });
        }
      }

      // Process all incoming HTTP details packets. Note that
      // Network event update packets are sent in batches from:
      // `WebConsoleOutputWrapper.dispatchMessageUpdate` using
      // NETWORK_MESSAGE_UPDATE action.
      // Make sure to call `dataProvider.onNetworkEventUpdate`
      // to fetch data from the backend.
      if (type == NETWORK_MESSAGE_UPDATE) {
        const { actor } = action.response.networkInfo;
        const open = getAllMessagesUiById(state).includes(actor);
        if (open) {
          const message = getMessage(state, actor);
          message.updates.forEach(updateType => {
            dataProvider.onNetworkEventUpdate({
              packet: { updateType },
              networkInfo: message,
            });
          });
        }
      }

      return newState;
    }

    return next(netProviderEnhancer, initialState, enhancer);
  };
}

module.exports = enableNetProvider;
