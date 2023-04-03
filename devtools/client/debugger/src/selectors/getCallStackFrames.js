/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedSource } from "./sources";
import { getBlackBoxRanges } from "./source-blackbox";
import { getCurrentThreadFrames } from "./pause";
import { annotateFrames } from "../utils/pause/frames";
import { isFrameBlackBoxed } from "../utils/source";
import { createSelector } from "reselect";

function getLocation(frame, isGeneratedSource) {
  return isGeneratedSource
    ? frame.generatedLocation || frame.location
    : frame.location;
}

function getSourceForFrame(frame, isGeneratedSource) {
  return getLocation(frame, isGeneratedSource).source;
}

function appendSource(frame, selectedSource) {
  const isGeneratedSource = selectedSource && !selectedSource.isOriginal;
  return {
    ...frame,
    location: getLocation(frame, isGeneratedSource),
    source: getSourceForFrame(frame, isGeneratedSource),
  };
}

export function formatCallStackFrames(
  frames,
  selectedSource,
  blackboxedRanges
) {
  if (!frames) {
    return null;
  }

  const formattedFrames = frames
    .filter(frame => getSourceForFrame(frame))
    .map(frame => appendSource(frame, selectedSource))
    .filter(frame => !isFrameBlackBoxed(frame, blackboxedRanges));

  return annotateFrames(formattedFrames);
}

export const getCallStackFrames = createSelector(
  getCurrentThreadFrames,
  getSelectedSource,
  getBlackBoxRanges,
  formatCallStackFrames
);
