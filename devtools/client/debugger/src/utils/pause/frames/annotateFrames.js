/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getLibraryFromUrl } from "./getLibraryFromUrl";

/**
 * Augment all frame objects with a 'library' attribute.
 */
export function annotateFramesWithLibrary(frames) {
  for (const frame of frames) {
    frame.library = getLibraryFromUrl(frame, frames);
  }

  // Babel need some special treatment to recognize some particular async stack pattern
  for (const idx of getBabelFrameIndexes(frames)) {
    const frame = frames[idx];
    frame.library = "Babel";
  }
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

  for (let index = 0, length = frames.length; index < length; index++) {
    const frame = frames[index];
    const frameUrl = frame.location.source.url;

    if (
      frame.displayName === "tryCatch" &&
      frameUrl.match(/regenerator-runtime/i)
    ) {
      startIndexes.push(index);
    }

    if (startIndexes.length > endIndexes.length) {
      if (frame.displayName === "flush" && frameUrl.match(/_microtask/i)) {
        endIndexes.push(index);
      }
      if (frame.displayName === "_asyncToGenerator/<") {
        endIndexes.push(index + 1);
      }
    }
  }

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
