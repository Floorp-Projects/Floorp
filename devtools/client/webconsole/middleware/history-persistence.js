/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  APPEND_TO_HISTORY,
  CLEAR_HISTORY,
  EVALUATE_EXPRESSION,
} = require("resource://devtools/client/webconsole/constants.js");

const historyActions = require("resource://devtools/client/webconsole/actions/history.js");

loader.lazyRequireGetter(
  this,
  "asyncStorage",
  "resource://devtools/shared/async-storage.js"
);

/**
 * History persistence middleware is responsible for loading
 * and maintaining history of executed expressions in JSTerm.
 */
function historyPersistenceMiddleware(webConsoleUI, store) {
  let historyLoaded = false;
  asyncStorage.getItem("webConsoleHistory").then(
    value => {
      if (Array.isArray(value)) {
        store.dispatch(historyActions.historyLoaded(value));
      }
      historyLoaded = true;
    },
    err => {
      historyLoaded = true;
      console.error(err);
    }
  );

  return next => action => {
    const res = next(action);

    const triggerStoreActions = [
      APPEND_TO_HISTORY,
      CLEAR_HISTORY,
      EVALUATE_EXPRESSION,
    ];

    // Save the current history entries when modified, but wait till
    // entries from the previous session are loaded.
    const { isPrivate } =
      webConsoleUI.hud?.commands?.targetCommand?.targetFront?.targetForm || {};

    if (
      !isPrivate &&
      historyLoaded &&
      triggerStoreActions.includes(action.type)
    ) {
      const state = store.getState();
      asyncStorage
        .setItem("webConsoleHistory", state.history.entries)
        .catch(e => {
          console.error("Error when saving WebConsole input history", e);
        });
    }

    return res;
  };
}

module.exports = historyPersistenceMiddleware;
