/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isStepping, getPauseReason, getThreadContext } from "../../selectors";
import { evaluateExpressions } from "../expressions";
import { inDebuggerEval } from "../../utils/pause";

import type { ThunkArgs } from "../types";
import type { ResumedPacket } from "../../client/firefox/types";

/**
 * Debugger has just resumed
 *
 * @memberof actions/pause
 * @static
 */
export function resumed(packet: ResumedPacket) {
  return async ({ dispatch, client, getState }: ThunkArgs) => {
    const thread = packet.from;
    const why = getPauseReason(getState(), thread);
    const wasPausedInEval = inDebuggerEval(why);
    const wasStepping = isStepping(getState(), thread);

    dispatch({ type: "RESUME", thread, wasStepping });

    const cx = getThreadContext(getState());
    if (!wasStepping && !wasPausedInEval && cx.thread == thread) {
      await dispatch(evaluateExpressions(cx));
    }
  };
}
