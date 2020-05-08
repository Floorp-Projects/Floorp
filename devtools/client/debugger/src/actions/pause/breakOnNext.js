/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "../types";
import type { PauseAction } from "../types/PauseAction";
import type { ThreadContext } from "../../types";

/**
 * Debugger breakOnNext command.
 * It's different from the comand action because we also want to
 * highlight the pause icon.
 *
 * @memberof actions/pause
 * @static
 */
export function breakOnNext(cx: ThreadContext): PauseAction {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    await client.breakOnNext(cx.thread);
    return dispatch({ type: "BREAK_ON_NEXT", thread: cx.thread });
  };
}
