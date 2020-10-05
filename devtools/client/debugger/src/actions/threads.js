/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { Target } from "../client/firefox/types";
import type { Action, ThunkArgs } from "./types";
import { removeSourceActors } from "./source-actors";
import { validateContext } from "../utils/context";

import { getContext, getThread, getSourceActorsForThread } from "../selectors";

export function addTarget(targetFront: Target) {
  return async function(args: ThunkArgs) {
    const { client, getState, dispatch } = args;
    const cx = getContext(getState());
    const thread = await client.addThread(targetFront);
    validateContext(getState(), cx);

    dispatch(({ type: "INSERT_THREAD", cx, newThread: thread }: Action));
  };
}

export function removeTarget(targetFront: Target) {
  return async function(args: ThunkArgs) {
    const { getState, client, dispatch } = args;
    const cx = getContext(getState());
    const thread = getThread(getState(), targetFront.targetForm.threadActor);

    if (!thread) {
      return;
    }

    client.removeThread(thread);
    const sourceActors = getSourceActorsForThread(getState(), thread.actor);
    dispatch(removeSourceActors(sourceActors));
    dispatch(
      ({
        type: "REMOVE_THREAD",
        cx,
        oldThread: thread,
      }: Action)
    );
  };
}

export function toggleJavaScriptEnabled(enabled: Boolean) {
  return async ({ panel, dispatch, client }: ThunkArgs) => {
    await client.toggleJavaScriptEnabled(enabled);
    dispatch({
      type: "TOGGLE_JAVASCRIPT_ENABLED",
      value: enabled,
    });
  };
}
