/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { selectLocation } from "../sources/index";
import { evaluateExpressions } from "../expressions";
import { fetchScopes } from "./fetchScopes";
import { validateSelectedFrame } from "../../utils/context";

/**
 * @memberof actions/pause
 * @static
 */
export function selectFrame(frame) {
  return async ({ dispatch, getState }) => {
    // Frames that aren't on-stack do not support evalling and may not
    // have live inspectable scopes, so we do not allow selecting them.
    if (frame.state !== "on-stack") {
      dispatch(selectLocation(frame.location));
      return;
    }

    dispatch({
      type: "SELECT_FRAME",
      frame,
    });

    // It's important that we wait for selectLocation to finish because
    // we rely on the source being loaded and symbols fetched below.
    await dispatch(selectLocation(frame.location));
    validateSelectedFrame(getState(), frame);

    await dispatch(evaluateExpressions(frame));

    await dispatch(fetchScopes(frame));
  };
}
