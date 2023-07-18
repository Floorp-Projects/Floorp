/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getFrames, getSymbols, getCurrentThread } from "../../selectors";

import { findClosestFunction } from "../../utils/ast";

/**
 * For frames related to non-WASM original sources, try to lookup for the function
 * name in the original source. The server will provide the function name in `displayName`
 * in the generated source, but not in the original source.
 * This information will be stored in frame's `originalDisplayName` attribute
 */
function mapDisplayName(frame, { getState }) {
  // Ignore WASM original frames
  if (frame.isOriginal) {
    return frame;
  }
  // When it is a regular (non original) JS source, the server already returns a valid displayName attribute.
  if (!frame.location.source.isOriginal) {
    return frame;
  }
  // Ignore the frame if we already computed this attribute.
  // Given that the action is called on every source selection,
  // we will recall this method on all frames for each selection.
  if (frame.originalDisplayName) {
    return frame;
  }

  const symbols = getSymbols(getState(), frame.location);

  if (!symbols || !symbols.functions) {
    return frame;
  }

  const originalFunction = findClosestFunction(symbols, frame.location);

  if (!originalFunction) {
    return frame;
  }

  const originalDisplayName = originalFunction.name;
  return { ...frame, originalDisplayName };
}

export function mapDisplayNames() {
  return function ({ dispatch, getState }) {
    const thread = getCurrentThread(getState());
    const frames = getFrames(getState(), thread);

    if (!frames) {
      return;
    }

    const mappedFrames = frames.map(frame =>
      mapDisplayName(frame, { getState })
    );

    dispatch({
      type: "MAP_FRAME_DISPLAY_NAMES",
      thread,
      frames: mappedFrames,
    });
  };
}
