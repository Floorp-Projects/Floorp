/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getGeneratedFrameScope,
  getOriginalFrameScope,
} from "../../selectors/index";
import { mapScopes } from "./mapScopes";
import { generateInlinePreview } from "./inlinePreview";
import { PROMISE } from "../utils/middleware/promise";

export function fetchScopes(selectedFrame) {
  return async function ({ dispatch, getState, client }) {
    // See if we already fetched the scopes.
    // We may have pause on multiple thread and re-select a paused thread
    // for which we already fetched the scopes.
    // Ignore pending scopes as the previous action may have been cancelled
    // by context assertions.
    let scopes = getGeneratedFrameScope(getState(), selectedFrame);
    if (!scopes?.scope) {
      scopes = dispatch({
        type: "ADD_SCOPES",
        selectedFrame,
        [PROMISE]: client.getFrameScopes(selectedFrame),
      });

      scopes.then(() => {
        dispatch(generateInlinePreview(selectedFrame));
      });
    }

    if (!getOriginalFrameScope(getState(), selectedFrame)) {
      await dispatch(mapScopes(selectedFrame, scopes));
    }
  };
}
