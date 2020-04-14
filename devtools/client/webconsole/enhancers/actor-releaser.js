/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MESSAGES_ADD,
  MESSAGES_CLEAR,
  PRIVATE_MESSAGES_CLEAR,
  MESSAGES_CLEAR_LOGPOINT,
  FRONTS_TO_RELEASE_CLEAR,
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

      const { type } = action;
      if (
        webConsoleUI &&
        [
          MESSAGES_ADD,
          MESSAGES_CLEAR,
          PRIVATE_MESSAGES_CLEAR,
          MESSAGES_CLEAR_LOGPOINT,
        ].includes(type)
      ) {
        const promises = [];
        state.messages.frontsToRelease.forEach(front => {
          // We only release the front if it actually has a release method, if it isn't
          // already destroyed, and if it's not in the sidebar (where we might still need it).
          if (
            front &&
            typeof front.release === "function" &&
            front.actorID &&
            (!state.ui.frontInSidebar ||
              state.ui.frontInSidebar.actorID !== front.actorID)
          ) {
            promises.push(front.release());
          }
        });

        // Emit an event we can listen to to make sure all the fronts were released.
        Promise.all(promises).then(() =>
          webConsoleUI.emitForTests("fronts-released")
        );

        // Reset `frontsToRelease` in message reducer.
        state = reducer(state, {
          type: FRONTS_TO_RELEASE_CLEAR,
        });
      }

      return state;
    }

    return next(releaseActorsEnhancer, initialState, enhancer);
  };
}

module.exports = enableActorReleaser;
