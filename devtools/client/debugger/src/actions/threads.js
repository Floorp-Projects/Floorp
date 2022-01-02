/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { removeSourceActors } from "./source-actors";
import { validateContext } from "../utils/context";

import { getContext, getThread, getSourceActorsForThread } from "../selectors";

export function addTarget(targetFront) {
  return async function(args) {
    const { client, getState, dispatch } = args;
    const cx = getContext(getState());
    const thread = await client.addThread(targetFront);
    validateContext(getState(), cx);

    dispatch({ type: "INSERT_THREAD", cx, newThread: thread });
  };
}

export function removeTarget(targetFront) {
  return async function(args) {
    const { getState, client, dispatch } = args;
    const cx = getContext(getState());
    const thread = getThread(getState(), targetFront.targetForm.threadActor);

    if (!thread) {
      return;
    }

    client.removeThread(thread);
    const sourceActors = getSourceActorsForThread(getState(), thread.actor);
    dispatch(removeSourceActors(sourceActors));
    dispatch({
      type: "REMOVE_THREAD",
      cx,
      oldThread: thread,
    });
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
