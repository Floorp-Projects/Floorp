/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs, ActionType } from "../../types";

/**
 * A middleware that allows thunks (functions) to be dispatched. If
 * it's a thunk, it is called with an argument that contains
 * `dispatch`, `getState`, and any additional args passed in via the
 * middleware constructure. This allows the action to create multiple
 * actions (most likely asynchronously).
 */
export function thunk(makeArgs: any) {
  return ({ dispatch, getState }: ThunkArgs) => {
    const args = { dispatch, getState };

    return (next: Function) => (action: ActionType) => {
      return typeof action === "function"
        ? action(makeArgs ? makeArgs(args, getState()) : args)
        : next(action);
    };
  };
}
