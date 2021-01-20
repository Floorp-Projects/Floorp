/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import Actions from "../../actions";

import sourceQueue from "../../utils/source-queue";
import { hasSourceActor } from "../../selectors";
import { stringToSourceActorId } from "../../reducers/source-actors";

type Dependencies = {
  actions: typeof Actions,
  store: any,
};

let store: any;

function setupEvents(dependencies: Dependencies): void {
  const actions = dependencies.actions;
  sourceQueue.initialize(actions);
  store = dependencies.store;
}

/**
 * This method wait for the given source is registered in Redux store.
 *
 * @param {String} sourceActor
 *                 Actor ID of the source to be waiting for.
 */
async function waitForSourceActorToBeRegisteredInStore(sourceActor: string) {
  const sourceActorId = stringToSourceActorId(sourceActor);
  if (!hasSourceActor(store.getState(), sourceActorId)) {
    await new Promise(resolve => {
      const unsubscribe = store.subscribe(check);
      let currentState = null;
      function check() {
        const previousState = currentState;
        currentState = store.getState().sourceActors.values;
        if (previousState == currentState) {
          return;
        }
        if (hasSourceActor(store.getState(), sourceActorId)) {
          unsubscribe();
          resolve();
        }
      }
    });
  }
}

export { setupEvents, waitForSourceActorToBeRegisteredInStore };
