/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getFrames, getSource, getSelectedFrame } from "../../selectors";

import assert from "../../utils/assert";

import type { Frame, OriginalFrame, ThreadContext } from "../../types";
import type { State } from "../../reducers/types";
import type { ThunkArgs } from "../types";

import SourceMaps, { isGeneratedId } from "devtools-source-map";

function isFrameBlackboxed(state, frame) {
  const source = getSource(state, frame.location.sourceId);
  return source?.isBlackBoxed;
}

function getSelectedFrameId(state, thread, frames) {
  let selectedFrame = getSelectedFrame(state, thread);
  if (selectedFrame && !isFrameBlackboxed(state, selectedFrame)) {
    return selectedFrame.id;
  }

  selectedFrame = frames.find(frame => !isFrameBlackboxed(state, frame));
  return selectedFrame?.id;
}

export function updateFrameLocation(
  frame: Frame,
  sourceMaps: typeof SourceMaps
) {
  if (frame.isOriginal) {
    return Promise.resolve(frame);
  }
  return sourceMaps.getOriginalLocation(frame.location).then(loc => ({
    ...frame,
    location: loc,
    generatedLocation: frame.generatedLocation || frame.location,
  }));
}

function updateFrameLocations(
  frames: Frame[],
  sourceMaps: typeof SourceMaps
): Promise<Frame[]> {
  if (!frames || frames.length == 0) {
    return Promise.resolve(frames);
  }

  return Promise.all(
    frames.map(frame => updateFrameLocation(frame, sourceMaps))
  );
}

function isWasmOriginalSourceFrame(
  frame: Frame,
  getState: () => State
): boolean {
  if (isGeneratedId(frame.location.sourceId)) {
    return false;
  }
  const generatedSource = getSource(
    getState(),
    frame.generatedLocation.sourceId
  );

  return Boolean(generatedSource?.isWasm);
}

async function expandFrames(
  frames: Frame[],
  sourceMaps: typeof SourceMaps,
  getState: () => State
): Promise<Frame[]> {
  const result = [];
  for (let i = 0; i < frames.length; ++i) {
    const frame = frames[i];
    if (frame.isOriginal || !isWasmOriginalSourceFrame(frame, getState)) {
      result.push(frame);
      continue;
    }
    const originalFrames: ?Array<OriginalFrame> = await sourceMaps.getOriginalStackFrames(
      frame.generatedLocation
    );
    if (!originalFrames) {
      result.push(frame);
      continue;
    }

    assert(originalFrames.length > 0, "Expected at least one original frame");
    // First entry has not specific location -- use one from original frame.
    originalFrames[0] = {
      ...originalFrames[0],
      location: frame.location,
    };

    originalFrames.forEach((originalFrame, j) => {
      if (!originalFrame.location) {
        return;
      }

      // Keep outer most frame with true actor ID, and generate uniquie
      // one for the nested frames.
      const id = j == 0 ? frame.id : `${frame.id}-originalFrame${j}`;
      result.push({
        id,
        displayName: originalFrame.displayName,
        location: originalFrame.location,
        index: frame.index,
        source: null,
        thread: frame.thread,
        scope: frame.scope,
        this: frame.this,
        isOriginal: true,
        // More fields that will be added by the mapDisplayNames and
        // updateFrameLocation.
        generatedLocation: frame.generatedLocation,
        originalDisplayName: originalFrame.displayName,
        originalVariables: originalFrame.variables,
        asyncCause: frame.asyncCause,
        state: frame.state,
      });
    });
  }
  return result;
}

/**
 * Map call stack frame locations and display names to originals.
 * e.g.
 * 1. When the debuggee pauses
 * 2. When a source is pretty printed
 * 3. When symbols are loaded
 * @memberof actions/pause
 * @static
 */
export function mapFrames(cx: ThreadContext) {
  return async function(thunkArgs: ThunkArgs) {
    const { dispatch, getState, sourceMaps } = thunkArgs;
    const frames = getFrames(getState(), cx.thread);
    if (!frames) {
      return;
    }

    let mappedFrames = await updateFrameLocations(frames, sourceMaps);

    mappedFrames = await expandFrames(mappedFrames, sourceMaps, getState);

    const selectedFrameId = getSelectedFrameId(
      getState(),
      cx.thread,
      mappedFrames
    );

    dispatch({
      type: "MAP_FRAMES",
      cx,
      thread: cx.thread,
      frames: mappedFrames,
      selectedFrameId,
    });
  };
}
