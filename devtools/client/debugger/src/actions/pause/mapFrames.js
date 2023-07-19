/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  getFrames,
  getBlackBoxRanges,
  getSelectedFrame,
  getSymbols,
} from "../../selectors";

import { isFrameBlackBoxed } from "../../utils/source";

import assert from "../../utils/assert";
import { getOriginalLocation } from "../../utils/source-maps";
import {
  debuggerToSourceMapLocation,
  sourceMapToDebuggerLocation,
} from "../../utils/location";
import { annotateFramesWithLibrary } from "../../utils/pause/frames/annotateFrames";
import { createWasmOriginalFrame } from "../../client/firefox/create";
import { getOriginalDisplayNameForOriginalLocation } from "../../reducers/pause";

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

async function updateFrameLocationAndDisplayName(frame, thunkArgs) {
  // Ignore WASM original sources
  if (frame.isOriginal) {
    return frame;
  }

  const location = await getOriginalLocation(frame.location, thunkArgs, true);
  // Avoid instantiating new frame objects if the frame location isn't mapped
  if (location == frame.location) {
    return frame;
  }

  // As we now know that this frame relates to an original source...
  // Check if we already fetched symbols for it and compute the frame's originalDisplayName.
  // Otherwise it will be lazily computed by the reducer on source selection / symbols being fetched.
  const symbols = getSymbols(thunkArgs.getState(), location);
  const originalDisplayName = symbols
    ? getOriginalDisplayNameForOriginalLocation(symbols, location)
    : null;

  // As we modify frame object, fork it to force causing re-renders
  return {
    ...frame,
    location,
    generatedLocation: frame.generatedLocation || frame.location,
    originalDisplayName,
  };
}

function isWasmOriginalSourceFrame(frame) {
  if (!frame.location.source.isOriginal) {
    return false;
  }

  return Boolean(frame.generatedLocation?.source.isWasm);
}

/**
 * Wasm Source Maps can come with an non-standard "xScopes" attribute
 * which allows mapping the scope of a given location.
 */
async function expandWasmFrames(frames, { getState, sourceMapLoader }) {
  const result = [];
  for (let i = 0; i < frames.length; ++i) {
    const frame = frames[i];
    if (frame.isOriginal || !isWasmOriginalSourceFrame(frame)) {
      result.push(frame);
      continue;
    }
    const originalFrames = await sourceMapLoader.getOriginalStackFrames(
      debuggerToSourceMapLocation(frame.generatedLocation)
    );
    if (!originalFrames) {
      result.push(frame);
      continue;
    }

    assert(!!originalFrames.length, "Expected at least one original frame");
    // First entry has no specific location -- use one from the generated frame.
    originalFrames[0].location = frame.location;

    originalFrames.forEach((originalFrame, j) => {
      if (!originalFrame.location) {
        return;
      }

      // Keep outer most frame with true actor ID, and generate unique
      // one for the nested frames.
      const id = j == 0 ? frame.id : `${frame.id}-originalFrame${j}`;
      const originalFrameLocation = sourceMapToDebuggerLocation(
        getState(),
        originalFrame.location
      );
      result.push(
        createWasmOriginalFrame(frame, id, originalFrame, originalFrameLocation)
      );
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
export function mapFrames(thread) {
  return async function (thunkArgs) {
    const { dispatch, getState } = thunkArgs;
    const frames = getFrames(getState(), thread);
    if (!frames || !frames.length) {
      return;
    }

    // Update frame's location/generatedLocation/originalDisplayNames in case it relates to an original source
    let mappedFrames = await Promise.all(
      frames.map(frame => updateFrameLocationAndDisplayName(frame, thunkArgs))
    );

    mappedFrames = await expandWasmFrames(mappedFrames, thunkArgs);

    // Add the "library" attribute on all frame objects (if relevant)
    annotateFramesWithLibrary(mappedFrames);

    // After having mapped the frames, we should update the selected frame
    // just in case the selected frame is now set on a blackboxed original source
    const selectedFrameId = getSelectedFrameId(
      getState(),
      thread,
      mappedFrames
    );

    dispatch({
      type: "MAP_FRAMES",
      thread,
      frames: mappedFrames,
      selectedFrameId,
    });
  };
}
