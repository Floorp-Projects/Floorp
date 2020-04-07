/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { flatMap, zip, range } from "lodash";

import type { Frame } from "../../../types";
import { getFrameUrl } from "./getFrameUrl";
import { getLibraryFromUrl } from "./getLibraryFromUrl";

type AnnotatedFrame =
  | {|
      ...Frame,
      library: string,
    |}
  | Frame;

export function annotateFrames(frames: Frame[]): AnnotatedFrame[] {
  const annotatedFrames = frames.map(f => annotateFrame(f, frames));
  return annotateBabelAsyncFrames(annotatedFrames);
}

function annotateFrame(frame: Frame, frames: Frame[]): AnnotatedFrame {
  const library = getLibraryFromUrl(frame, frames);
  if (library) {
    return { ...frame, library };
  }

  return frame;
}

function annotateBabelAsyncFrames(frames: Frame[]): Frame[] {
  const babelFrameIndexes = getBabelFrameIndexes(frames);
  const isBabelFrame = frameIndex => babelFrameIndexes.includes(frameIndex);

  return frames.map((frame, frameIndex) =>
    isBabelFrame(frameIndex) ? { ...frame, library: "Babel" } : frame
  );
}

// Receives an array of frames and looks for babel async
// call stack groups.
function getBabelFrameIndexes(frames) {
  const startIndexes = frames.reduce((accumulator, frame, index) => {
    if (
      getFrameUrl(frame).match(/regenerator-runtime/i) &&
      frame.displayName === "tryCatch"
    ) {
      return [...accumulator, index];
    }
    return accumulator;
  }, []);

  const endIndexes = frames.reduce((accumulator, frame, index) => {
    if (
      getFrameUrl(frame).match(/_microtask/i) &&
      frame.displayName === "flush"
    ) {
      return [...accumulator, index];
    }
    if (frame.displayName === "_asyncToGenerator/<") {
      return [...accumulator, index + 1];
    }
    return accumulator;
  }, []);

  if (startIndexes.length != endIndexes.length || startIndexes.length === 0) {
    return frames;
  }

  // Receives an array of start and end index tuples and returns
  // an array of async call stack index ranges.
  // e.g. [[1,3], [5,7]] => [[1,2,3], [5,6,7]]
  // $FlowIgnore
  return flatMap(zip(startIndexes, endIndexes), ([startIndex, endIndex]) =>
    range(startIndex, endIndex + 1)
  );
}
