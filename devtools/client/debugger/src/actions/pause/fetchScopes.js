/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSelectedFrame,
  getGeneratedFrameScope,
  getOriginalFrameScope,
} from "../../selectors";
import { mapScopes } from "./mapScopes";
import { generateInlinePreview } from "./inlinePreview";
import { PROMISE } from "../utils/middleware/promise";

export function fetchScopes(cx) {
  return async function ({ dispatch, getState, client }) {
    const frame = getSelectedFrame(getState(), cx.thread);
    // Ignore the request if there is no selected frame
    // as scopes are related to a particular frame.
    if (!frame) {
      return;
    }
    // See if we already fetched the scopes.
    // We may have pause on multiple thread and re-select a paused thread
    // for which we already fetched the scopes.
    // Ignore pending scopes as the previous action may have been cancelled
    // by context assertions.
    let scopes = getGeneratedFrameScope(getState(), frame);
    if (!scopes?.scope) {
      scopes = dispatch({
        type: "ADD_SCOPES",
        cx,
        thread: cx.thread,
        frame,
        [PROMISE]: client.getFrameScopes(frame),
      });

      scopes.then(() => {
        dispatch(generateInlinePreview(cx, frame));
      });
    }

    if (!getOriginalFrameScope(getState(), frame)) {
      await dispatch(mapScopes(cx, scopes, frame));
    }
  };
}
