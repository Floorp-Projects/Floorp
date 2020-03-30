/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { selectLocation } from "../sources";
import { getContext, getSourceByURL } from "../../selectors";
import type { ThunkArgs } from "../types";
import type { URL } from "../../types";

type Location = {
  sourceUrl: URL,
  column: number,
  line: number,
};

export function previewPausedLocation(location: Location) {
  return ({ dispatch, getState }: ThunkArgs) => {
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
