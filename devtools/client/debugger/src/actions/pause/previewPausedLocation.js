/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { selectLocation } from "../sources";
import { getContext, getSourceByURL } from "../../selectors";

export function previewPausedLocation(location) {
  return ({ dispatch, getState }) => {
    const cx = getContext(getState());
    const source = getSourceByURL(getState(), location.sourceUrl);
    if (!source) {
      return;
    }

    const sourceLocation = {
      line: location.line,
      column: location.column,
      sourceId: source.id,
    };
    dispatch(selectLocation(cx, sourceLocation));

    dispatch({
      type: "PREVIEW_PAUSED_LOCATION",
      location: sourceLocation,
    });
  };
}

export function clearPreviewPausedLocation() {
  return {
    type: "CLEAR_PREVIEW_PAUSED_LOCATION",
  };
}
