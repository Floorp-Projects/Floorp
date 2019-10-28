/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ActorId, ExecutionPoint } from "../../types";
import type { ThunkArgs } from "../types";

export function setFramePositions(
  positions: Array<ExecutionPoint>,
  frame: ActorId,
  thread: ActorId
) {
  return ({ dispatch }: ThunkArgs) => {
    dispatch({
      type: "SET_FRAME_POSITIONS",
      positions,
      frame,
      thread,
    });
  };
}
