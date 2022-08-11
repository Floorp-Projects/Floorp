/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createThread } from "../client/firefox/create";

export function addTarget(targetFront) {
  return { type: "INSERT_THREAD", newThread: createThread(targetFront) };
}

export function removeTarget(targetFront) {
  return {
    type: "REMOVE_THREAD",
    threadActorID: targetFront.targetForm.threadActor,
  };
}

export function toggleJavaScriptEnabled(enabled) {
  return async ({ panel, dispatch, client }) => {
    await client.toggleJavaScriptEnabled(enabled);
    dispatch({
      type: "TOGGLE_JAVASCRIPT_ENABLED",
      value: enabled,
    });
  };
}
