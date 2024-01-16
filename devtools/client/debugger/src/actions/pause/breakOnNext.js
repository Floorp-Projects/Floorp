/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getCurrentThread } from "../../selectors";
/**
 * Debugger breakOnNext command.
 * It's different from the comand action because we also want to
 * highlight the pause icon.
 *
 * @memberof actions/pause
 * @static
 */
export function breakOnNext() {
  return async ({ dispatch, getState, client }) => {
    const thread = getCurrentThread(getState());
    await client.breakOnNext(thread);
    return dispatch({ type: "BREAK_ON_NEXT", thread });
  };
}
