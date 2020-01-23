/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { selectLocation } from "../sources";
import { evaluateExpressions } from "../expressions";
import { fetchScopes } from "./fetchScopes";
import assert from "../../utils/assert";
import { getCanRewind } from "../../reducers/threads";

import type { Frame, ThreadContext } from "../../types";
import type { ThunkArgs } from "../types";

/**
 * @memberof actions/pause
 * @static
 */
export function selectFrame(cx: ThreadContext, frame: Frame) {
  return async ({ dispatch, client, getState, sourceMaps }: ThunkArgs) => {
    assert(cx.thread == frame.thread, "Thread mismatch");

    // Frames with an async cause are not selected
    if (frame.asyncCause) {
      return dispatch(selectLocation(cx, frame.location));
    }

    dispatch({
      type: "SELECT_FRAME",
      cx,
      thread: cx.thread,
      frame,
    });

    if (getCanRewind(getState())) {
      client.fetchAncestorFramePositions(frame.index);
    }

    dispatch(selectLocation(cx, frame.location));

    dispatch(evaluateExpressions(cx));
    dispatch(fetchScopes(cx));
  };
}
