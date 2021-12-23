/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getFrameUrl } from "./getFrameUrl";
import { getLibraryFromUrl } from "./getLibraryFromUrl";

export function annotateFrames(frames) {
  const annotatedFrames = frames.map(f => annotateFrame(f, frames));
  return annotateBabelAsyncFrames(annotatedFrames);
}

function annotateFrame(frame, frames) {
  const library = getLibraryFromUrl(frame, frames);
  if (library) {
    return { ...frame, library };
  }

  return frame;
}

function annotateBabelAsyncFrames(frames) {
  const babelFrameIndexes = getBabelFrameIndexes(frames);
  const isBabelFrame = frameIndex => babelFrameIndexes.includes(frameIndex);

  return frames.map((frame, frameIndex) =>
    isBabelFrame(frameIndex) ? { ...frame, library: "Babel" } : frame
  );
}

/**
 * Returns all the indexes that are part of a babel async call stack.
 *
 * @param {Array<Object>} frames
 * @returns Array<Integer>
 */
function getBabelFrameIndexes(frames) {
  const startIndexes = [];
  const endIndexes = [];

  frames.forEach((frame, index) => {
    const frameUrl = getFrameUrl(frame);

    if (
      frameUrl.match(/regenerator-runtime/i) &&
      frame.displayName === "tryCatch"
    ) {
      startIndexes.push(index);
    }
    if (frame.displayName === "flush" && frameUrl.match(/_microtask/i)) {
      endIndexes.push(index);
    }
    if (frame.displayName === "_asyncToGenerator/<") {
      endIndexes.push(index + 1);
    }
  });

  if (startIndexes.length != endIndexes.length || startIndexes.length === 0) {
    return [];
  }

  const babelFrameIndexes = [];
  // We have the same number of start and end indexes, we can loop through one of them to
  // build our async call stack index ranges
  // e.g. if we have startIndexes: [1,5] and endIndexes: [3,8], we want to return [1,2,3,5,6,7,8]
  startIndexes.forEach((startIndex, index) => {
    const matchingEndIndex = endIndexes[index];
    for (let i = startIndex; i <= matchingEndIndex; i++) {
      babelFrameIndexes.push(i);
    }
  });
  return babelFrameIndexes;
}
