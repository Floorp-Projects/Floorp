/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPEND_TO_HISTORY,
  CLEAR_HISTORY,
} = require("devtools/client/webconsole/constants");

const historyActions = require("devtools/client/webconsole/actions/history");

loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

/**
 * History persistence middleware is responsible for loading
 * and maintaining history of executed expressions in JSTerm.
 */
function historyPersistenceMiddleware(store) {
  let historyLoaded = false;
  asyncStorage.getItem("webConsoleHistory").then(value => {
    if (Array.isArray(value)) {
      store.dispatch(historyActions.historyLoaded(value));
    }
    historyLoaded = true;
  }, err => {
    historyLoaded = true;
    console.error(err);
  });

  return next => action => {
    const res = next(action);

    const triggerStoreActions = [
      APPEND_TO_HISTORY,
      CLEAR_HISTORY,
    ];

    // Save the current history entries when modified, but wait till
    // entries from the previous session are loaded.
    if (historyLoaded && triggerStoreActions.includes(action.type)) {
      const state = store.getState();
      asyncStorage.setItem("webConsoleHistory", state.history.entries);
    }

    return res;
  };
}

module.exports = historyPersistenceMiddleware;
