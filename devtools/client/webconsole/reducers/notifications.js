/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  APPEND_NOTIFICATION,
  REMOVE_NOTIFICATION,
} = require("resource://devtools/client/webconsole/constants.js");

loader.lazyRequireGetter(
  this,
  ["appendNotification", "removeNotificationWithValue"],
  "resource://devtools/client/shared/components/NotificationBox.js",
  true
);

/**
 * Create default initial state for this reducer. The state is composed
 * from list of notifications.
 */
function getInitialState() {
  return {
    notifications: undefined,
  };
}

/**
 * Reducer function implementation. This reducers is responsible
 * for maintaining list of notifications. It's consumed by
 * `NotificationBox` component.
 */
function notifications(state = getInitialState(), action) {
  switch (action.type) {
    case APPEND_NOTIFICATION:
      return append(state, action);
    case REMOVE_NOTIFICATION:
      return remove(state, action);
  }

  return state;
}

// Helpers

function append(state, action) {
  return appendNotification(state, action);
}

function remove(state, action) {
  return removeNotificationWithValue(state.notifications, action.value);
}

// Exports

module.exports = {
  notifications,
};
