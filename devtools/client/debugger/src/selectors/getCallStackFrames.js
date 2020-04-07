/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getSources,
  getSelectedSource,
  getSourceInSources,
} from "../reducers/sources";
import { getCurrentThreadFrames } from "../reducers/pause";
import { annotateFrames } from "../utils/pause/frames";
import { isOriginal } from "../utils/source";
import type { State, SourceResourceState } from "../reducers/types";
import type { Frame, Source } from "../types";
import { createSelector } from "reselect";

function getLocation(frame, isGeneratedSource) {
  return isGeneratedSource
    ? frame.generatedLocation || frame.location
    : frame.location;
}

function getSourceForFrame(
  sources: SourceResourceState,
  frame: Frame,
  isGeneratedSource
): ?Source {
  const sourceId = getLocation(frame, isGeneratedSource).sourceId;
  return getSourceInSources(sources, sourceId);
}

function appendSource(
  sources: SourceResourceState,
  frame: Frame,
  selectedSource: ?Source
): Frame {
  const isGeneratedSource = selectedSource && !isOriginal(selectedSource);
  return {
    ...frame,
    location: getLocation(frame, isGeneratedSource),
    source: getSourceForFrame(sources, frame, isGeneratedSource),
  };
}

export function formatCallStackFrames(
  frames: Frame[],
  sources: SourceResourceState,
  selectedSource: Source
): ?(Frame[]) {
  if (!frames) {
    return null;
  }

  const formattedFrames: Frame[] = frames
    .filter(frame => getSourceForFrame(sources, frame))
    .map(frame => appendSource(sources, frame, selectedSource))
    .filter(frame => !frame?.source?.isBlackBoxed);

  return annotateFrames(formattedFrames);
}

// eslint-disable-next-line
export const getCallStackFrames: State => Frame[] = (createSelector: any)(
  getCurrentThreadFrames,
  getSources,
  getSelectedSource,
  formatCallStackFrames
);
