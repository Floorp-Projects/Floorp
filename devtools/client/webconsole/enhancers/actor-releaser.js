/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MESSAGES_ADD,
  MESSAGES_CLEAR,
  PRIVATE_MESSAGES_CLEAR,
  MESSAGES_CLEAR_LOGPOINT,
  REMOVED_ACTORS_CLEAR,
} = require("devtools/client/webconsole/constants");

/**
 * This enhancer is responsible for releasing actors on the backend.
 * When messages with arguments are removed from the store we should also
 * clean up the backend.
 */
function enableActorReleaser(webConsoleUI) {
  return next => (reducer, initialState, enhancer) => {
    function releaseActorsEnhancer(state, action) {
      state = reducer(state, action);

      const type = action.type;
      if (
        webConsoleUI &&
        [
          MESSAGES_ADD,
          MESSAGES_CLEAR,
          PRIVATE_MESSAGES_CLEAR,
          MESSAGES_CLEAR_LOGPOINT,
        ].includes(type)
      ) {
        state.messages.removedActors.forEach(actor =>
          webConsoleUI.releaseActor(actor)
        );

        // Reset `removedActors` in message reducer.
        state = reducer(state, {
          type: REMOVED_ACTORS_CLEAR,
        });
      }

      return state;
    }

    return next(releaseActorsEnhancer, initialState, enhancer);
  };
}

module.exports = enableActorReleaser;
