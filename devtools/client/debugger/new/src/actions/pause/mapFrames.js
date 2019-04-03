/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  getFrames,
  getSymbols,
  getSource,
  getSelectedFrame
} from "../../selectors";

import assert from "../../utils/assert";
import { findClosestFunction } from "../../utils/ast";

import type { Frame, ThreadId } from "../../types";
import type { State } from "../../reducers/types";
import type { ThunkArgs } from "../types";

import { isGeneratedId } from "devtools-source-map";

function isFrameBlackboxed(state, frame) {
  const source = getSource(state, frame.location.sourceId);
  return source && source.isBlackBoxed;
}

function getSelectedFrameId(state, thread, frames) {
  let selectedFrame = getSelectedFrame(state, thread);
  if (selectedFrame && !isFrameBlackboxed(state, selectedFrame)) {
    return selectedFrame.id;
  }

  selectedFrame = frames.find(frame => !isFrameBlackboxed(state, frame));
  return selectedFrame && selectedFrame.id;
}

export function updateFrameLocation(frame: Frame, sourceMaps: any) {
  if (frame.isOriginal) {
    return Promise.resolve(frame);
  }
  return sourceMaps.getOriginalLocation(frame.location).then(loc => ({
    ...frame,
    location: loc,
    generatedLocation: frame.generatedLocation || frame.location
  }));
}

function updateFrameLocations(
  frames: Frame[],
  sourceMaps: any
): Promise<Frame[]> {
  if (!frames || frames.length == 0) {
    return Promise.resolve(frames);
  }

  return Promise.all(
    frames.map(frame => updateFrameLocation(frame, sourceMaps))
  );
}

export function mapDisplayNames(
  frames: Frame[],
  getState: () => State
): Frame[] {
  return frames.map(frame => {
    if (frame.isOriginal) {
      return frame;
    }
    const source = getSource(getState(), frame.location.sourceId);

    if (!source) {
      return frame;
    }

    const symbols = getSymbols(getState(), source);

    if (!symbols || !symbols.functions) {
      return frame;
    }

    const originalFunction = findClosestFunction(symbols, frame.location);

    if (!originalFunction) {
      return frame;
    }

    const originalDisplayName = originalFunction.name;
    return { ...frame, originalDisplayName };
  });
}

function isWasmOriginalSourceFrame(frame, getState: () => State): boolean {
  if (isGeneratedId(frame.location.sourceId)) {
    return false;
  }
  const generatedSource = getSource(
    getState(),
    frame.generatedLocation.sourceId
  );

  return Boolean(generatedSource && generatedSource.isWasm);
}

async function expandFrames(
  frames: Frame[],
  sourceMaps: any,
  getState: () => State
): Promise<Frame[]> {
  const result = [];
  for (let i = 0; i < frames.length; ++i) {
    const frame = frames[i];
    if (frame.isOriginal || !isWasmOriginalSourceFrame(frame, getState)) {
      result.push(frame);
      continue;
    }
    const originalFrames = await sourceMaps.getOriginalStackFrames(
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
      location: frame.location
    };

    originalFrames.forEach((originalFrame, j) => {
      // Keep outer most frame with true actor ID, and generate uniquie
      // one for the nested frames.
      const id = j == 0 ? frame.id : `${frame.id}-originalFrame${j}`;
      result.push({
        id,
        thread: originalFrame.thread,
        displayName: originalFrame.displayName,
        location: originalFrame.location,
        source: null,
        scope: frame.scope,
        this: frame.this,
        isOriginal: true,
        // More fields that will be added by the mapDisplayNames and
        // updateFrameLocation.
        generatedLocation: frame.generatedLocation,
        originalDisplayName: originalFrame.displayName
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
export function mapFrames(thread: ThreadId) {
  return async function({ dispatch, getState, sourceMaps }: ThunkArgs) {
    const frames = getFrames(getState(), thread);
    if (!frames) {
      return;
    }

    let mappedFrames = await updateFrameLocations(frames, sourceMaps);
    mappedFrames = await expandFrames(mappedFrames, sourceMaps, getState);
    mappedFrames = mapDisplayNames(mappedFrames, getState);

    const selectedFrameId = getSelectedFrameId(
      getState(),
      thread,
      mappedFrames
    );
    dispatch({
      type: "MAP_FRAMES",
      thread,
      frames: mappedFrames,
      selectedFrameId
    });
  };
}
