/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { selectLocation } from "../sources";
import { evaluateExpressions } from "../expressions";
import { fetchScopes } from "./fetchScopes";
import assert from "../../utils/assert";

/**
 * @memberof actions/pause
 * @static
 */
export function selectFrame(cx, frame) {
  return async ({ dispatch, client, getState, sourceMaps }) => {
    assert(cx.thread == frame.thread, "Thread mismatch");

    // Frames that aren't on-stack do not support evalling and may not
    // have live inspectable scopes, so we do not allow selecting them.
    if (frame.state !== "on-stack") {
      dispatch(selectLocation(cx, frame.location));
      return;
    }

    dispatch({
      type: "SELECT_FRAME",
      cx,
      thread: cx.thread,
      frame,
    });

    // It's important that we wait for selectLocation to finish because
    // we rely on the source being loaded and symbols fetched below.
    await dispatch(selectLocation(cx, frame.location));

    dispatch(evaluateExpressions(cx));
    dispatch(fetchScopes(cx));
  };
}
