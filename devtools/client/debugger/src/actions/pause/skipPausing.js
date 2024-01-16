/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSkipPausing } from "../../selectors/index";

/**
 * @memberof actions/pause
 * @static
 */
export function toggleSkipPausing() {
  return async ({ dispatch, client, getState }) => {
    const skipPausing = !getSkipPausing(getState());
    await client.setSkipPausing(skipPausing);
    dispatch({ type: "TOGGLE_SKIP_PAUSING", skipPausing });
  };
}

/**
 * @memberof actions/pause
 * @static
 */
export function setSkipPausing(skipPausing) {
  return async ({ dispatch, client, getState }) => {
    const currentlySkipping = getSkipPausing(getState());
    if (currentlySkipping === skipPausing) {
      return;
    }

    await client.setSkipPausing(skipPausing);
    dispatch({ type: "TOGGLE_SKIP_PAUSING", skipPausing });
  };
}
