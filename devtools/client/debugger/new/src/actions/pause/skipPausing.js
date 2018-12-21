/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "../types";
import { getSkipPausing, getCurrentThread } from "../../selectors";

/**
 * @memberof actions/pause
 * @static
 */
export function toggleSkipPausing() {
  return async ({ dispatch, client, getState, sourceMaps }: ThunkArgs) => {
    const thread = getCurrentThread(getState());
    const skipPausing = !getSkipPausing(getState());
    await client.setSkipPausing(thread, skipPausing);
    dispatch({ type: "TOGGLE_SKIP_PAUSING", thread, skipPausing });
  };
}
