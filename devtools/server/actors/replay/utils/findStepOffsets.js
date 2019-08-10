/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getNeighbors(frame, offset, rewinding) {
  return rewinding
    ? frame.script.getPredecessorOffsets(offset)
    : frame.script.getSuccessorOffsets(offset);
}

/**
 * When replaying, we need to specify the offsets where a frame's onStep hook
 * should fire. Given that we are either stepping forward or backwards,
 * return an array of all the step targets
 * that could be reached next from startLocation.
 */
function findStepOffsets(frame, rewinding) {
  const seen = [];
  const result = [];
  let worklist = getNeighbors(frame, frame.offset, rewinding);

  while (worklist.length) {
    const offset = worklist.pop();
    if (seen.includes(offset)) {
      continue;
    }
    seen.push(offset);
    const meta = frame.script.getOffsetMetadata(offset);
    if (meta.isStepStart) {
      if (!result.includes(offset)) {
        result.push(offset);
      }
    } else {
      const neighbors = getNeighbors(frame, offset, rewinding);
      worklist = [...worklist, ...neighbors];
    }
  }

  return result;
}
exports.findStepOffsets = findStepOffsets;
