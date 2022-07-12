/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSourcesMap, getSelectedSource } from "./sources";
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

function getSourceForFrame(sourcesMap, frame, isGeneratedSource) {
  const sourceId = getLocation(frame, isGeneratedSource).sourceId;
  return sourcesMap.get(sourceId);
}

function appendSource(sourcesMap, frame, selectedSource) {
  const isGeneratedSource = selectedSource && !selectedSource.isOriginal;
  return {
    ...frame,
    location: getLocation(frame, isGeneratedSource),
    source: getSourceForFrame(sourcesMap, frame, isGeneratedSource),
  };
}

export function formatCallStackFrames(
  frames,
  sourcesMap,
  selectedSource,
  blackboxedRanges
) {
  if (!frames) {
    return null;
  }

  const formattedFrames = frames
    .filter(frame => getSourceForFrame(sourcesMap, frame))
    .map(frame => appendSource(sourcesMap, frame, selectedSource))
    .filter(frame => !isFrameBlackBoxed(frame, frame.source, blackboxedRanges));

  return annotateFrames(formattedFrames);
}

// eslint-disable-next-line
export const getCallStackFrames = (createSelector)(
  getCurrentThreadFrames,
  getSourcesMap,
  getSelectedSource,
  getBlackBoxRanges,
  formatCallStackFrames
);
