/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getIsPaused } from "../../selectors";

export function fetchFrames(thread) {
  return async function ({ dispatch, client, getState }) {
    let frames;
    try {
      frames = await client.getFrames(thread);
    } catch (e) {
      // getFrames will fail if the thread has resumed. In this case the thread
      // should no longer be valid and the frames we would have fetched would be
      // discarded anyways.
      if (getIsPaused(getState(), thread)) {
        throw e;
      }
    }
    dispatch({ type: "FETCHED_FRAMES", thread, frames });
  };
}
