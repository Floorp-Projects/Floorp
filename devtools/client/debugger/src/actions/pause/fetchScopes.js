/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getSelectedFrame, getGeneratedFrameScope } from "../../selectors";
import { mapScopes } from "./mapScopes";
import { generateInlinePreview } from "./inlinePreview";
import { PROMISE } from "../utils/middleware/promise";
import type { ThreadContext } from "../../types";
import type { ThunkArgs } from "../types";

export function fetchScopes(cx: ThreadContext) {
  return async function({ dispatch, getState, client, sourceMaps }: ThunkArgs) {
    const frame = getSelectedFrame(getState(), cx.thread);
    if (!frame || getGeneratedFrameScope(getState(), frame.id)) {
      return;
    }

    const scopes = dispatch({
      type: "ADD_SCOPES",
      cx,
      thread: cx.thread,
      frame,
      [PROMISE]: client.getFrameScopes(frame),
    });

    scopes.then(() => {
      dispatch(generateInlinePreview(cx, frame));
    });
    await dispatch(mapScopes(cx, scopes, frame));
  };
}
