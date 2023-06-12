/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getShouldSelectOriginalLocation } from "./sources";
import { getBlackBoxRanges } from "./source-blackbox";
import { getCurrentThreadFrames } from "./pause";
import { isFrameBlackBoxed } from "../utils/source";
import { createSelector } from "reselect";

/**
 * Only when:
 *  - the debugger is not showing original locations by default (shouldSelectOriginalLocation = false),
 *  - the frame is mapped (generatedLocation defined and different from location)
 *
 * hack frame's 'location' attribute to refer to its generated location.
 * This helps frontend code to always read frame.location without having to fallback to
 * frame.generatedLocation based on shouldSelectOriginalLocation value...
 * To the cost of cloning many frame objects!
 */
function hackFrameLocation(frame, shouldSelectOriginalLocation) {
  if (
    !shouldSelectOriginalLocation &&
    frame.generatedLocation &&
    frame.generatedLocation != frame.location
  ) {
    return {
      ...frame,
      location: frame.generatedLocation,
    };
  }
  return frame;
}

export function formatCallStackFrames(
  frames,
  shouldSelectOriginalLocation,
  blackboxedRanges
) {
  if (!frames) {
    return null;
  }

  return frames
    .map(frame => hackFrameLocation(frame, shouldSelectOriginalLocation))
    .filter(frame => !isFrameBlackBoxed(frame, blackboxedRanges));
}

export const getCallStackFrames = createSelector(
  getCurrentThreadFrames,
  getShouldSelectOriginalLocation,
  getBlackBoxRanges,
  formatCallStackFrames
);
