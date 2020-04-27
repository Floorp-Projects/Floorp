/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_BLOCKED_URL,
  REMOVE_BLOCKED_URL,
  TOGGLE_BLOCKED_URL,
  UPDATE_BLOCKED_URL,
  TOGGLE_BLOCKING_ENABLED,
  DISABLE_MATCHING_URLS,
  ENABLE_ALL_BLOCKED_URLS,
  DISABLE_ALL_BLOCKED_URLS,
  REMOVE_ALL_BLOCKED_URLS,
  REQUEST_BLOCKING_UPDATE_COMPLETE,
} = require("devtools/client/netmonitor/src/constants");

const BLOCKING_EVENTS = [
  ADD_BLOCKED_URL,
  REMOVE_BLOCKED_URL,
  TOGGLE_BLOCKED_URL,
  UPDATE_BLOCKED_URL,
  TOGGLE_BLOCKING_ENABLED,
  DISABLE_MATCHING_URLS,
  ENABLE_ALL_BLOCKED_URLS,
  DISABLE_ALL_BLOCKED_URLS,
  REMOVE_ALL_BLOCKED_URLS,
];

/**
 * This middleware is responsible for syncing the list of blocking patterns/urls with the backed.
 * It utilizes the connector object and `setBlockedUrls` function to sent the current list to the server
 * every time it's been modified.
 */
function requestBlockingMiddleware(connector) {
  return store => next => async action => {
    const res = next(action);

    if (BLOCKING_EVENTS.includes(action.type)) {
      const { blockedUrls, blockingEnabled } = store.getState().requestBlocking;
      const urls = blockingEnabled
        ? blockedUrls.reduce((arr, { enabled, url }) => {
            if (enabled) {
              arr.push(url);
            }
            return arr;
          }, [])
        : [];
      await connector.setBlockedUrls(urls);
      store.dispatch({ type: REQUEST_BLOCKING_UPDATE_COMPLETE });
    }
    return res;
  };
}

module.exports = requestBlockingMiddleware;
