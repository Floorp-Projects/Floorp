/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createThread } from "../client/firefox/create";
import { getSourcesToRemoveForThread } from "../selectors";

export function addTarget(targetFront) {
  return { type: "INSERT_THREAD", newThread: createThread(targetFront) };
}

export function removeTarget(targetFront) {
  return ({ getState, dispatch, parserWorker }) => {
    const threadActorID = targetFront.targetForm.threadActor;

    // Just before emitting the REMOVE_THREAD action,
    // synchronously compute the list of source and source actor objects
    // which should be removed as that one target get removed.
    //
    // The list of source objects isn't trivial to compute as these objects
    // are shared across targets/threads.
    const { actors, sources } = getSourcesToRemoveForThread(
      getState(),
      threadActorID
    );

    dispatch({
      type: "REMOVE_THREAD",
      threadActorID,
      actors,
      sources,
    });

    parserWorker.clearSources(sources.map(source => source.id));
  };
}

export function toggleJavaScriptEnabled(enabled) {
  return async ({ dispatch, client }) => {
    await client.toggleJavaScriptEnabled(enabled);
    dispatch({
      type: "TOGGLE_JAVASCRIPT_ENABLED",
      value: enabled,
    });
  };
}
