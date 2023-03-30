/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getSource,
  getFrames,
  getBlackBoxRanges,
  getSelectedFrame,
} from "../../selectors";

import { isFrameBlackBoxed } from "../../utils/source";

import assert from "../../utils/assert";
import { getOriginalLocation } from "../../utils/source-maps";
import { createLocation } from "../../utils/location";

import { isGeneratedId } from "devtools/client/shared/source-map-loader/index";

function getSelectedFrameId(state, thread, frames) {
  let selectedFrame = getSelectedFrame(state, thread);
  const blackboxedRanges = getBlackBoxRanges(state);

  if (selectedFrame && !isFrameBlackBoxed(selectedFrame, blackboxedRanges)) {
    return selectedFrame.id;
  }

  selectedFrame = frames.find(frame => {
    return !isFrameBlackBoxed(frame, blackboxedRanges);
  });
  return selectedFrame?.id;
}

async function updateFrameLocation(frame, thunkArgs) {
  if (frame.isOriginal) {
    return Promise.resolve(frame);
  }
  const location = await getOriginalLocation(frame.location, thunkArgs, true);
  return {
    ...frame,
    location,
    generatedLocation: frame.generatedLocation || frame.location,
  };
}

function updateFrameLocations(frames, thunkArgs) {
  if (!frames || !frames.length) {
    return Promise.resolve(frames);
  }

  return Promise.all(
    frames.map(frame => updateFrameLocation(frame, thunkArgs))
  );
}

function isWasmOriginalSourceFrame(frame, getState) {
  if (isGeneratedId(frame.location.sourceId)) {
    return false;
  }

  return Boolean(frame.generatedLocation?.source.isWasm);
}

async function expandFrames(frames, { getState, sourceMapLoader }) {
  const result = [];
  for (let i = 0; i < frames.length; ++i) {
    const frame = frames[i];
    if (frame.isOriginal || !isWasmOriginalSourceFrame(frame, getState)) {
      result.push(frame);
      continue;
    }
    const originalFrames = await sourceMapLoader.getOriginalStackFrames(
      frame.generatedLocation
    );
    if (!originalFrames) {
      result.push(frame);
      continue;
    }

    assert(!!originalFrames.length, "Expected at least one original frame");
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
        // SourceMapLoader doesn't known about debugger's source objects
        // so that we have to fetch it from here
        location: createLocation({
          ...originalFrame.location,
          source: getSource(getState(), originalFrame.location.sourceId),
        }),
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
export function mapFrames(cx) {
  return async function(thunkArgs) {
    const { dispatch, getState } = thunkArgs;
    const frames = getFrames(getState(), cx.thread);
    if (!frames) {
      return;
    }

    let mappedFrames = await updateFrameLocations(frames, thunkArgs);

    mappedFrames = await expandFrames(mappedFrames, thunkArgs);

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
