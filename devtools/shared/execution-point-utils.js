/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Utilities for working with execution points and breakpoint positions, which
// are used when replaying to identify points in the execution where the
// debugger can stop or where events of interest have occurred.
//
// A breakpoint position describes where breakpoints can be installed when
// replaying, and has the following properties:
//
// kind: The kind of breakpoint, which has one of the following values:
//   "Break": Break at an offset in a script.
//   "OnStep": Break at an offset in a script with a given frame depth.
//   "OnPop": Break when a script's frame with a given frame depth is popped.
//   "EnterFrame": Break when any script is entered.
//
// script: For all kinds but "EnterFrame", the ID of the position's script.
//
// offset: For "Break" and "OnStep", the offset within the script.
//
// frameIndex: For "OnStep" and "OnPop", the index of the topmost script frame.
//   Indexes start at zero for the first frame pushed, and increase with the
//   depth of the frame.
//
// An execution point is a unique identifier for a point in the recording where
// the debugger can pause, and has the following properties:
//
// checkpoint: ID of the most recent checkpoint.
//
// progress: Value of the progress counter when the point is reached.
//
// position: Optional breakpoint position where the pause occurs at. This cannot
//   have the "Break" kind (see below) and is missing if the execution point is
//   at the checkpoint itself.
//
// The above properties must uniquely identify a single point in the recording.
// This property is ensured mainly through how the progress counter is managed.
// The position in the point must not be able to execute multiple times without
// the progress counter being incremented.
//
// The progress counter is incremented whenever a frame is pushed onto the stack
// or a loop entry point is reached. Because it increments when looping, a
// specific JS frame cannot reach the same position multiple times with the same
// progress counter. Because it increments when pushing frames, different JS
// frames with the same frame depth cannot reach the same position multiple
// times with the same progress counter. The position must specify the frame
// depth for this argument to hold, so "Break" positions are not used in
// execution points. They are used when the user-specified breakpoints are being
// installed, though, and when pausing the execution point will use the
// appropriate "OnStep" position for the frame depth.

// Return whether pointA happens before pointB in the recording.
function pointPrecedes(pointA, pointB) {
  if (pointA.checkpoint != pointB.checkpoint) {
    return pointA.checkpoint < pointB.checkpoint;
  }
  if (pointA.progress != pointB.progress) {
    return pointA.progress < pointB.progress;
  }

  const posA = pointA.position;
  const posB = pointB.position;

  // Except when we're at a checkpoint, all execution points have positions.
  // Because the progress counter is bumped when executing script, points with
  // the same checkpoint and progress counter will either both be at that
  // checkpoint, or both be at an intra-checkpoint point.
  assert(!!posA == !!posB);
  if (!posA || positionEquals(posA, posB)) {
    return false;
  }

  // If an execution point doesn't have a frame index (i.e. EnterFrame) then it
  // has bumped the progress counter and predates everything else that is
  // associated with the same progress counter.
  if ("frameIndex" in posA != "frameIndex" in posB) {
    return "frameIndex" in posB;
  }

  // Only certain execution point kinds do not bump the progress counter.
  assert(posA.kind == "OnStep" || posA.kind == "OnPop");
  assert(posB.kind == "OnStep" || posB.kind == "OnPop");

  // Deeper frames predate shallower frames, if the progress counter is the
  // same. We bump the progress counter when pushing frames, but not when
  // popping them.
  assert("frameIndex" in posA && "frameIndex" in posB);
  if (posA.frameIndex != posB.frameIndex) {
    return posA.frameIndex > posB.frameIndex;
  }

  // Within a frame, OnStep points come before OnPop points.
  if (posA.kind != posB.kind) {
    return posA.kind == "OnStep";
  }

  // Earlier script locations predate later script locations.
  assert("offset" in posA && "offset" in posB);
  return posA.offset < posB.offset;
}

// Return whether two execution points are the same.
// eslint-disable-next-line no-unused-vars
function pointEquals(pointA, pointB) {
  return !pointPrecedes(pointA, pointB) && !pointPrecedes(pointB, pointA);
}

// Return whether two breakpoint positions are the same.
function positionEquals(posA, posB) {
  return posA.kind == posB.kind
      && posA.script == posB.script
      && posA.offset == posB.offset
      && posA.frameIndex == posB.frameIndex;
}

// Return whether an execution point matching posB also matches posA.
// eslint-disable-next-line no-unused-vars
function positionSubsumes(posA, posB) {
  if (positionEquals(posA, posB)) {
    return true;
  }

  if (posA.kind == "Break" && posB.kind == "OnStep" &&
      posA.script == posB.script && posA.offset == posB.offset) {
    return true;
  }

  return false;
}

function assert(v) {
  if (!v) {
    dump(`Assertion failed: ${Error().stack}\n`);
    throw new Error("Assertion failed!");
  }
}

this.EXPORTED_SYMBOLS = [
  "pointPrecedes",
  "pointEquals",
  "positionEquals",
  "positionSubsumes",
];
