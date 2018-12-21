/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getCurrentThread, isStepping, getPauseReason } from "../../selectors";
import { evaluateExpressions } from "../expressions";
import { inDebuggerEval } from "../../utils/pause";

import type { ThunkArgs } from "../types";

/**
 * Debugger has just resumed
 *
 * @memberof actions/pause
 * @static
 */
export function resumed() {
  return async ({ dispatch, client, getState }: ThunkArgs) => {
    const why = getPauseReason(getState());
    const wasPausedInEval = inDebuggerEval(why);
    const wasStepping = isStepping(getState());
    const thread = getCurrentThread(getState());

    dispatch({ type: "RESUME", thread });

    if (!wasStepping && !wasPausedInEval) {
      await dispatch(evaluateExpressions());
    }
  };
}
