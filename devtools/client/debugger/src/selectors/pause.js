// @flow
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getCurrentThread } from "../reducers/pause";
import { getSelectedLocation } from "../reducers/sources";

// eslint-disable-next-line
import { getSelectedLocation as _getSelectedLocation } from "../utils/selected-location";
import { createSelector } from "reselect";

import type { Frame, SourceLocation, ThreadId } from "../types";
import type { Selector, State } from "../reducers/types";

export const getSelectedFrames: Selector<{ [string]: ?Frame }> = createSelector(
  state => state.pause.threads,
  threadPauseState => {
    const selectedFrames = {};
    for (const thread in threadPauseState) {
      const pausedThread = threadPauseState[thread];
      const { selectedFrameId, frames } = pausedThread;
      if (frames) {
        selectedFrames[thread] = frames.find(
          frame => frame.id == selectedFrameId
        );
      }
    }
    return selectedFrames;
  }
);

export function getSelectedFrame(state: State, thread: ThreadId): ?Frame {
  const selectedFrames = getSelectedFrames(state);
  return selectedFrames[thread];
}

export const getVisibleSelectedFrame: Selector<?{
  id: string,
  location: SourceLocation,
}> = createSelector(
  getSelectedLocation,
  getSelectedFrames,
  getCurrentThread,
  (selectedLocation, selectedFrames, thread) => {
    const selectedFrame = selectedFrames[thread];
    if (!selectedFrame) {
      return null;
    }

    const { id, displayName } = selectedFrame;

    return {
      id,
      displayName,
      location: _getSelectedLocation(selectedFrame, selectedLocation),
    };
  }
);
