/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThreadContext } from "../../types";
import type { ThunkArgs } from "../types";

export function fetchFrames(cx: ThreadContext) {
  return async function({ dispatch, client }: ThunkArgs) {
    const { thread } = cx;
    const frames = await client.getFrames(thread);
    dispatch({ type: "FETCHED_FRAMES", thread, frames, cx });
  };
}
