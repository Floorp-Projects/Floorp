/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// This file provides an interface which the ReplayDebugger uses to interact
// with a middleman's child recording and replaying processes. There can be
// several child processes in existence at once; this is largely hidden from the
// ReplayDebugger, and the position of each child is managed to provide a fast
// and stable experience when rewinding or running forward.

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")());
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
  "Components.utils.import('resource://gre/modules/Services.jsm');" +
  "Components.utils.import('resource://devtools/shared/execution-point-utils.js');" +
  "addDebuggerToGlobal(this);",
  sandbox
);
const {
  RecordReplayControl,
  Services,
  pointPrecedes,
  pointEquals,
  positionEquals,
  positionSubsumes,
} = sandbox;

const InvalidCheckpointId = 0;
const FirstCheckpointId = 1;

// Application State Control
//
// This section describes the strategy used for managing child processes so that
// we can be responsive to user interactions. There is at most one recording
// child process, and one or more replaying child processes.
//
// The recording child cannot rewind: it only runs forward and adds new data to
// the recording. If we are paused or trying to pause at a place within the
// recording, then the recording child is also paused.
//
// To manage the replaying children, we identify a set of checkpoints that will
// be saved by some replaying child. The duration between saved checkpoints
// should be roughly equal, and they must be sufficiently closely spaced that
// any point in the recording can be quickly reached by some replaying child
// restoring the previous saved checkpoint and then running forward to the
// target point.
//
// As we identify the saved checkpoints, each is assigned to some replaying
// child, which is responsible for both saving that checkpoint and for scanning
// the contents of the recording from that checkpoint until the next saved
// checkpoint.
//
// When adding new data to the recording, replaying children will scan and save
// the regions and checkpoints they are responsible for, and will otherwise play
// forward as far as they can in the recording. We always want to have one or
// more replaying children that are at far end of the recording and able to
// start scanning/saving as the recording grows. In order to ensure this,
// consider the following:
//
// - Replaying must be faster than recording. While recording there is idle time
//   as we wait for user input, timers, etc. that is not present while
//   replaying. Suppose that replaying speed is F times the recording speed.
//   F must be less than one.
//
// - Scanning and saving a section of the recording is slower than recording.
//   Both of these have a lot of overhead, and suppose that scanning is S times
//   the recording speed. S will be more than one.
//
// - When there is more than one replaying child, each child can divide its time
//   between scanning/saving and simply replaying. We want its average speed to
//   match that of the recording. If there are N replaying children, each child
//   scans 1/N of the recording, so the average speed compared to the recording
//   is S/N + F*(N-1)/N. We want this term to be one or slightly less.
//
// For example, if F = 0.75 and S = 3, then replaying is 33% faster than
// recording, and scanning is three times slower than recording. If N = 4,
// the average speed is 3/4 + 0.75*3/4 = 1.31 times that of recording, which
// will cause the replaying processes to fall further and further behind the
// recording. If N = 12, the average speed is 3/12 + 0.75*11/12 = 0.94 times
// that of the recording, which will allow the replaying processes to keep up
// with the recording.
//
// Eventually we'll want to do this analysis dynamically to scale up or throttle
// back the number of active replaying processes. For now, though, we rely on
// a fixed number of replaying processes, and hope that it is enough.

////////////////////////////////////////////////////////////////////////////////
// Child Processes
////////////////////////////////////////////////////////////////////////////////

// Information about a child recording or replaying process.
function ChildProcess(id, recording) {
  this.id = id;

  // Whether this process is recording.
  this.recording = recording;

  // Whether this process is paused.
  this.paused = false;

  // The last point we paused at.
  this.lastPausePoint = null;

  // Manifests which this child needs to send asynchronously.
  this.asyncManifests = [];

  // All checkpoints which this process has saved or will save, which is a
  // subset of all the saved checkpoints.
  this.savedCheckpoints = new Set(recording ? [] : [FirstCheckpointId]);

  // All saved checkpoints whose region of the recording has been scanned by
  // this child.
  this.scannedCheckpoints = new Set();

  // Checkpoints in savedCheckpoints which haven't been sent to the child yet.
  this.needSaveCheckpoints = [];

  // Whether this child has diverged from the recording and cannot run forward.
  this.divergedFromRecording = false;

  // Any manifest which is currently being executed. Child processes initially
  // have a manifest to run forward to the first checkpoint.
  this.manifest = {
    onFinished: ({ point }) => {
      if (this == gMainChild) {
        getCheckpointInfo(FirstCheckpointId).point = point;
        Services.tm.dispatchToMainThread(recording ? maybeResumeRecording : setMainChild);
      }
    },
  };
}

ChildProcess.prototype = {
  // Get the execution point where this child is currently paused.
  pausePoint() {
    assert(this.paused);
    return this.lastPausePoint;
  },

  // Get the checkpoint where this child is currently paused.
  pauseCheckpoint() {
    const point = this.pausePoint();
    assert(!point.position);
    return point.checkpoint;
  },

  // Send a manifest to paused child to execute. The child unpauses while
  // executing the manifest, and pauses again when it finishes. Manifests have
  // the following properties:
  //
  // contents: The JSON object to send to the child describing the operation.
  // onFinished: A callback which is called after the manifest finishes with the
  //   manifest's result.
  sendManifest(manifest) {
    assert(this.paused);
    this.paused = false;
    this.manifest = manifest;

    dumpv(`SendManifest #${this.id} ${JSON.stringify(manifest.contents)}`);
    RecordReplayControl.sendManifest(this.id, manifest.contents);
  },

  // Called when the child's current manifest finishes.
  manifestFinished(response) {
    assert(!this.paused);
    if (response && response.point) {
      this.lastPausePoint = response.point;
    }
    this.paused = true;
    this.manifest.onFinished(response);
    this.manifest = null;
  },

  // Block until this child is paused. If maybeCreateCheckpoint is specified
  // then a checkpoint is created if this child is recording, so that it pauses
  // quickly (otherwise it could sit indefinitely if there is no page activity).
  waitUntilPaused(maybeCreateCheckpoint) {
    if (this.paused) {
      return;
    }
    RecordReplayControl.waitUntilPaused(this.id, maybeCreateCheckpoint);
    assert(this.paused);
  },

  // Add a checkpoint for this child to save.
  addSavedCheckpoint(checkpoint) {
    dumpv(`AddSavedCheckpoint #${this.id} ${checkpoint}`);
    this.savedCheckpoints.add(checkpoint);
    if (checkpoint != FirstCheckpointId) {
      this.needSaveCheckpoints.push(checkpoint);
    }
  },

  // Get any checkpoints to inform the child that it needs to save.
  flushNeedSaveCheckpoints() {
    const rv = this.needSaveCheckpoints;
    this.needSaveCheckpoints = [];
    return rv;
  },

  // Send a manifest to this child asynchronously. The child does not need to be
  // paused, and will process async manifests in the order they were added.
  // Async manifests can end up being reassigned to a different child. This
  // returns a promise that resolves when the manifest finishes. Async manifests
  // have the following properties:
  //
  // shouldSkip: Optional callback invoked with the executing child when it is
  //   about to be sent. Returns true if the manifest should not be sent, and
  //   the promise resolved immediately.
  //
  // contents: Callback invoked with the executing child when it is being sent.
  //   Returns the contents to send to the child.
  //
  // onFinished: Optional callback invoked with the executing child and manifest
  //   response after the manifest finishes.
  //
  // noReassign: Optional boolean which can be set to prevent the manifest from
  //   being reassigned to another child.
  //
  // The optional point parameter specifies an execution point which the child
  // should be paused at before executing the manifest. Otherwise it could be
  // paused anywhere. The returned value is the child which ended up executing
  // the manifest.
  sendManifestAsync(manifest, point) {
    pokeChildSoon(this);
    return new Promise(resolve => {
      this.asyncManifests.push({ resolve, manifest, point });
    });
  },

  // Return true if progress was made while executing the next async manifest.
  processAsyncManifest() {
    if (this.asyncManifests.length == 0) {
      return false;
    }
    const { resolve, manifest, point } = this.asyncManifests[0];
    if (manifest.shouldSkip && manifest.shouldSkip(this)) {
      resolve(this);
      this.asyncManifests.shift();
      pokeChildSoon(this);
      return true;
    }

    // If this is the active child then we can't process arbitrary manifests.
    // Only handle those which cannot be reassigned, and hand off others to
    // random other children.
    if (this == gActiveChild && !manifest.noReassign) {
      const child = pickReplayingChild();
      child.asyncManifests.push(this.asyncManifests.shift());
      pokeChildSoon(child);
      pokeChildSoon(this);
      return true;
    }

    if (point && maybeReachPoint(this, point)) {
      return true;
    }
    this.sendManifest({
      contents: manifest.contents(this),
      onFinished: data => {
        if (manifest.onFinished) {
          manifest.onFinished(this, data);
        }
        resolve(this);
        pokeChildSoon(this);
      },
    });
    this.asyncManifests.shift();
    return true;
  },
};

// Child which is always at the end of the recording. When there is a recording
// child this is it, and when we are replaying an old execution this is a
// replaying child that is unable to rewind and is used in the same way as the
// recording child.
let gMainChild;

// Replaying children available for exploring the interior of the recording,
// indexed by their ID.
const gReplayingChildren = [];

function lookupChild(id) {
  if (id == gMainChild.id) {
    return gMainChild;
  }
  assert(gReplayingChildren[id]);
  return gReplayingChildren[id];
}

// ID of the last replaying child we picked for an operation.
let lastPickedChildId = 0;

function pickReplayingChild() {
  // Use a round robin approach when picking new children for operations,
  // to try to keep activity among the children evenly distributed.
  while (true) {
    lastPickedChildId = (lastPickedChildId + 1) % gReplayingChildren.length;
    const child = gReplayingChildren[lastPickedChildId];
    if (child) {
      return child;
    }
  }
}

// The singleton ReplayDebugger, or undefined if it doesn't exist.
let gDebugger;

////////////////////////////////////////////////////////////////////////////////
// Application State
////////////////////////////////////////////////////////////////////////////////

// Any child the user is interacting with, which may be paused or not.
let gActiveChild = null;

// Information about each checkpoint, indexed by the checkpoint's id.
const gCheckpoints = [ null ];

function CheckpointInfo() {
  // The time taken to run from this checkpoint to the next one, excluding idle
  // time.
  this.duration = 0;

  // Execution point at the checkpoint.
  this.point = null;

  // If the checkpoint is saved, the replaying child responsible for saving it
  // and scanning the region up to the next saved checkpoint.
  this.owner = null;
}

function getCheckpointInfo(id) {
  while (id >= gCheckpoints.length) {
    gCheckpoints.push(new CheckpointInfo());
  }
  return gCheckpoints[id];
}

// How much execution time has elapsed since a checkpoint.
function timeSinceCheckpoint(id) {
  let time = 0;
  for (let i = id ? id : FirstCheckpointId; i < gCheckpoints.length; i++) {
    time += gCheckpoints[i].duration;
  }
  return time;
}

// The checkpoint up to which the recording runs.
let gLastFlushCheckpoint = InvalidCheckpointId;

// The last saved checkpoint.
let gLastSavedCheckpoint = FirstCheckpointId;

// How often we want to flush the recording.
const FlushMs = .5 * 1000;

// How often we want to save a checkpoint.
const SavedCheckpointMs = .25 * 1000;

function addSavedCheckpoint(checkpoint) {
  if (getCheckpointInfo(checkpoint).owner) {
    return;
  }

  const owner = pickReplayingChild();
  getCheckpointInfo(checkpoint).owner = owner;
  owner.addSavedCheckpoint(checkpoint);
  gLastSavedCheckpoint = checkpoint;
}

function addCheckpoint(checkpoint, duration) {
  assert(!getCheckpointInfo(checkpoint).duration);
  getCheckpointInfo(checkpoint).duration = duration;

  // Mark saved checkpoints as required, unless we haven't spawned any replaying
  // children yet.
  if (timeSinceCheckpoint(gLastSavedCheckpoint) >= SavedCheckpointMs &&
      gReplayingChildren.length > 0) {
    addSavedCheckpoint(checkpoint + 1);
  }
}

function ownerChild(checkpoint) {
  assert(checkpoint <= gLastSavedCheckpoint);
  while (!getCheckpointInfo(checkpoint).owner) {
    checkpoint--;
  }
  return getCheckpointInfo(checkpoint).owner;
}

// Unpause a child and restore it to its most recent saved checkpoint at or
// before target.
function restoreCheckpoint(child, target) {
  while (!child.savedCheckpoints.has(target)) {
    target--;
  }
  child.sendManifest({
    contents: { kind: "restoreCheckpoint", target },
    onFinished({ restoredCheckpoint }) {
      assert(restoredCheckpoint);
      child.divergedFromRecording = false;
      pokeChildSoon(child);
    },
  });
}

// Bring a child to the specified execution point, sending it one or more
// manifests if necessary. Returns true if the child has not reached the point
// yet but some progress was made, or false if the child is at the point.
function maybeReachPoint(child, endpoint) {
  if (pointEquals(child.pausePoint(), endpoint) && !child.divergedFromRecording) {
    return false;
  }
  if (child.divergedFromRecording || child.pausePoint().position) {
    restoreCheckpoint(child, child.pausePoint().checkpoint);
    return true;
  }
  if (endpoint.checkpoint < child.pauseCheckpoint()) {
    restoreCheckpoint(child, endpoint.checkpoint);
    return true;
  }
  child.sendManifest({
    contents: {
      kind: "runToPoint",
      endpoint,
      needSaveCheckpoints: child.flushNeedSaveCheckpoints(),
    },
    onFinished() {
      pokeChildSoon(child);
    },
  });
  return true;
}

function nextSavedCheckpoint(checkpoint) {
  assert(gCheckpoints[checkpoint].owner);
  // eslint-disable-next-line no-empty
  while (!gCheckpoints[++checkpoint].owner) {}
  return checkpoint;
}

function forSavedCheckpointsInRange(start, end, callback) {
  assert(gCheckpoints[start].owner);
  for (let checkpoint = start;
       checkpoint < end;
       checkpoint = nextSavedCheckpoint(checkpoint)) {
    callback(checkpoint);
  }
}

function getSavedCheckpoint(checkpoint) {
  while (!gCheckpoints[checkpoint].owner) {
    checkpoint--;
  }
  return checkpoint;
}

// Get the execution point to use for a checkpoint.
function checkpointExecutionPoint(checkpoint) {
  return gCheckpoints[checkpoint].point;
}

// Check to see if an idle replaying child can make any progress.
function pokeChild(child) {
  assert(!child.recording);

  if (!child.paused) {
    return;
  }

  if (child.processAsyncManifest()) {
    return;
  }

  if (child == gActiveChild) {
    sendChildToPausePoint(child);
    return;
  }

  // If there is nothing to do, run forward to the end of the recording.
  maybeReachPoint(child, checkpointExecutionPoint(gLastFlushCheckpoint));
}

function pokeChildSoon(child) {
  Services.tm.dispatchToMainThread(() => pokeChild(child));
}

function pokeChildren() {
  for (const child of gReplayingChildren) {
    if (child) {
      pokeChild(child);
    }
  }
}

function pokeChildrenSoon() {
  Services.tm.dispatchToMainThread(() => pokeChildren());
}

////////////////////////////////////////////////////////////////////////////////
// Search State
////////////////////////////////////////////////////////////////////////////////

// All currently installed breakpoints.
const gBreakpoints = [];

// Recording Scanning
//
// Scanning a section of the recording between two neighboring saved checkpoints
// allows the execution points for each script breakpoint position to be queried
// by sending a manifest to the child which performed the scan.

// Ensure the region for a saved checkpoint has been scanned by some child,
// returning a promise that resolves with that child.
function scanRecording(checkpoint) {
  assert(checkpoint < gLastFlushCheckpoint);

  for (const child of gReplayingChildren) {
    if (child && child.scannedCheckpoints.has(checkpoint)) {
      return child;
    }
  }

  const initialChild = ownerChild(checkpoint);
  const endpoint = nextSavedCheckpoint(checkpoint);
  return initialChild.sendManifestAsync({
    shouldSkip: child => child.scannedCheckpoints.has(checkpoint),
    contents(child) {
      return {
        kind: "scanRecording",
        endpoint,
        needSaveCheckpoints: child.flushNeedSaveCheckpoints(),
      };
    },
    onFinished: child => child.scannedCheckpoints.add(checkpoint),
  }, checkpointExecutionPoint(checkpoint));
}

// Map from saved checkpoints to information about breakpoint hits within the
// range of that checkpoint.
const gHitSearches = new Map();

// Only hits on script locations (Break and OnStep positions) can be found by
// scanning the recording.
function canFindHits(position) {
  return position.kind == "Break" || position.kind == "OnStep";
}

// Find all hits on the specified position between a saved checkpoint and the
// following saved checkpoint, using data from scanning the recording. This
// returns a promise that resolves with the resulting hits.
async function findHits(checkpoint, position) {
  assert(canFindHits(position));
  assert(gCheckpoints[checkpoint].owner);

  if (!gHitSearches.has(checkpoint)) {
    gHitSearches.set(checkpoint, []);
  }

  // Check if we already have the hits.
  if (!gHitSearches.has(checkpoint)) {
    gHitSearches.set(checkpoint, []);
  }
  const checkpointHits = gHitSearches.get(checkpoint);
  let hits = findExistingHits();
  if (hits) {
    return hits;
  }

  const child = await scanRecording(checkpoint);
  const endpoint = nextSavedCheckpoint(checkpoint);
  await child.sendManifestAsync({
    shouldSkip: () => findExistingHits() != null,
    contents() {
      return {
        kind: "findHits",
        position,
        startpoint: checkpoint,
        endpoint,
      };
    },
    onFinished: (_, hits) => checkpointHits.push({ position, hits }),
    // findHits has to be sent to the child which scanned this portion of the
    // recording. It can be sent to the active child, though, because it
    // does not have side effects.
    noReassign: true,
  });

  hits = findExistingHits();
  assert(hits);
  return hits;

  function findExistingHits() {
    const entry = checkpointHits.find(({ position: existingPosition, hits }) => {
      return positionEquals(position, existingPosition);
    });
    return entry ? entry.hits : null;
  }
}

// Frame Steps
//
// When the recording scanning is not sufficient to figure out where to stop
// when resuming, the steps for the currently paused frame can be fetched. This
// mainly helps with finding the targets for EnterFrame breakpoints used when
// stepping in, and will be used in the future to improve stepping performance.
//
// The steps for a frame are the list of execution points for breakpoint
// positions traversed when executing a particular script frame, from the
// initial EnterFrame to the final OnPop. The steps also include the EnterFrame
// execution points for any direct callees of the frame.

// All steps for frames which have been determined.
const gFrameSteps = [];

// When there are stepping breakpoints installed, we need to know the steps in
// the current frame in order to find the next or previous hit.
function hasSteppingBreakpoint() {
  return gBreakpoints.some(bp => bp.kind == "EnterFrame" || bp.kind == "OnPop");
}

// Find all the steps in the frame which point is part of. This returns a
// promise that resolves with the steps that were found.
async function findFrameSteps(point) {
  if (!point.position) {
    return null;
  }

  assert(point.position.kind == "EnterFrame" ||
         point.position.kind == "OnStep" ||
         point.position.kind == "OnPop");

  let steps = findExistingSteps();
  if (steps) {
    return steps;
  }

  const savedCheckpoint = getSavedCheckpoint(point.checkpoint);

  let entryPoint;
  if (point.position.kind == "EnterFrame") {
    entryPoint = point;
  } else {
    // The point is in the interior of the frame. Figure out the initial
    // EnterFrame point for the frame.
    const {
      progress: targetProgress,
      position: { script, frameIndex: targetFrameIndex },
    } = point;

    // Find a position for the entry point of the frame.
    const { firstBreakpointOffset } = gControl.sendRequestMainChild({
      type: "getScript",
      id: script,
    });
    const entryPosition = {
      kind: "OnStep",
      script,
      offset: firstBreakpointOffset,
      frameIndex: targetFrameIndex,
    };

    const entryHits = await findHits(savedCheckpoint, entryPosition);

    // Find the last hit on the entry position before the target point, which must
    // be the entry point of the frame containing the target point. Since frames
    // do not span checkpoints the hit must be in the range we are searching. Note
    // that we are not dealing with async/generator frames very well here.
    let progressAtFrameStart = 0;
    for (const { progress, position: { frameIndex } } of entryHits) {
      if (frameIndex == targetFrameIndex &&
          progress <= targetProgress &&
          progress > progressAtFrameStart) {
        progressAtFrameStart = progress;
      }
    }
    assert(progressAtFrameStart);

    // The progress at the initial offset should be the same as at the
    // EnterFrame which pushed the frame onto the stack. No scripts should be
    // able to run between these two points, though we don't have a way to check
    // this.
    entryPoint = {
      checkpoint: point.checkpoint,
      progress: progressAtFrameStart,
      position: { kind: "EnterFrame" },
    };
  }

  const child = ownerChild(savedCheckpoint);
  await child.sendManifestAsync({
    shouldSkip: () => findExistingSteps() != null,
    contents() {
      return { kind: "findFrameSteps", entryPoint };
    },
    onFinished: (_, { frameSteps }) => gFrameSteps.push(frameSteps),
  }, entryPoint);

  steps = findExistingSteps();
  assert(steps);
  return steps;

  function findExistingSteps() {
    // Frame steps will include EnterFrame for both the initial and callee
    // frames, so the same point can appear in two sets of steps. In this case
    // the EnterFrame needs to be the first step.
    if (point.position.kind == "EnterFrame") {
      return gFrameSteps.find(steps => pointEquals(point, steps[0]));
    }
    return gFrameSteps.find(steps => steps.some(p => pointEquals(point, p)));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Pause State
////////////////////////////////////////////////////////////////////////////////

// The pause mode classifies the current state of the debugger.
const PauseModes = {
  // Process is actively recording. gPausePoint is the last point the main child
  // reached.
  RUNNING: "RUNNING",

  // gActiveChild is paused at gPausePoint.
  PAUSED: "PAUSED",

  // gActiveChild is being taken to gPausePoint, after which we will pause.
  ARRIVING: "ARRIVING",

  // gActiveChild is null, and we are looking for the last breakpoint hit prior
  // to or following gPausePoint, at which we will pause.
  RESUMING_BACKWARD: "RESUMING_BACKWARD",
  RESUMING_FORWARD: "RESUMING_FORWARD",
};

// Current pause mode.
let gPauseMode = PauseModes.RUNNING;

// In PAUSED or ARRIVING mode, the point we are paused at or sending the active child to.
let gPausePoint = null;

// In PAUSED mode, any debugger requests that have been sent to the child.
const gDebuggerRequests = [];

function setPauseState(mode, point, child) {
  assert(mode);
  const idString = child ? ` #${child.id}` : "";
  dumpv(`SetPauseState ${mode} ${JSON.stringify(point)}${idString}`);

  gPauseMode = mode;
  gPausePoint = point;
  gActiveChild = child;

  pokeChildrenSoon();
}

// Asynchronously send a child to the specific point and pause the debugger.
function setReplayingPauseTarget(point) {
  setPauseState(PauseModes.ARRIVING, point, ownerChild(point.checkpoint));
  gDebuggerRequests.length = 0;

  findFrameSteps(point);
}

// Synchronously send a child to the specific point and pause.
function pauseReplayingChild(point) {
  const child = ownerChild(point.checkpoint);

  do {
    child.waitUntilPaused();
  } while (maybeReachPoint(child, point));

  setPauseState(PauseModes.PAUSED, point, child);

  findFrameSteps(point);
}

function sendChildToPausePoint(child) {
  assert(child.paused && child == gActiveChild);

  switch (gPauseMode) {
  case PauseModes.PAUSED:
    assert(pointEquals(child.pausePoint(), gPausePoint));
    return;

  case PauseModes.ARRIVING:
    if (pointEquals(child.pausePoint(), gPausePoint)) {
      setPauseState(PauseModes.PAUSED, gPausePoint, gActiveChild);
      gDebugger._onPause();
      return;
    }
    maybeReachPoint(child, gPausePoint);
    return;

  default:
    throw new Error(`Unexpected pause mode: ${gPauseMode}`);
  }
}

// After the debugger resumes, find the point where it should pause next.
async function finishResume() {
  assert(gPauseMode == PauseModes.RESUMING_FORWARD ||
         gPauseMode == PauseModes.RESUMING_BACKWARD);
  const forward = gPauseMode == PauseModes.RESUMING_FORWARD;

  let startCheckpoint = gPausePoint.checkpoint;
  if (!forward && !gPausePoint.position) {
    startCheckpoint--;
  }
  startCheckpoint = getSavedCheckpoint(startCheckpoint);

  let checkpoint = startCheckpoint;
  for (;; forward ? checkpoint++ : checkpoint--) {
    if (checkpoint == gMainChild.pauseCheckpoint()) {
      // We searched the entire space forward to the end of the recording and
      // didn't find any breakpoint hits, so resume recording.
      assert(forward);
      setPauseState(PauseModes.RUNNING, null, gMainChild);
      maybeResumeRecording();
      return;
    }

    if (checkpoint == InvalidCheckpointId) {
      // We searched backward to the beginning of the recording, so restore the
      // first checkpoint.
      assert(!forward);
      setReplayingPauseTarget(checkpointExecutionPoint(FirstCheckpointId));
      return;
    }

    if (!gCheckpoints[checkpoint].owner) {
      continue;
    }

    let hits = [];

    // Find any breakpoint hits in this region of the recording.
    for (const bp of gBreakpoints) {
      if (canFindHits(bp)) {
        const bphits = await findHits(checkpoint, bp);
        hits = hits.concat(bphits);
      }
    }

    // When there are stepping breakpoints, look for breakpoint hits in the
    // steps for the current frame.
    if (checkpoint == startCheckpoint && hasSteppingBreakpoint()) {
      const steps = await findFrameSteps(gPausePoint);
      hits = hits.concat(steps.filter(point => {
        return gBreakpoints.some(bp => positionSubsumes(bp, point.position));
      }));
    }

    if (forward) {
      hits = hits.filter(p => pointPrecedes(gPausePoint, p));
    } else {
      hits = hits.filter(p => pointPrecedes(p, gPausePoint));
    }

    if (hits.length) {
      // We've found the point where the search should end.
      hits.sort((a, b) => forward ? pointPrecedes(b, a) : pointPrecedes(a, b));
      setReplayingPauseTarget(hits[0]);
      return;
    }
  }
}

// Unpause the active child and asynchronously pause at the next or previous
// breakpoint hit.
function resume(forward) {
  if (gActiveChild.recording) {
    if (forward) {
      maybeResumeRecording();
      return;
    }
  }
  if (gPausePoint.checkpoint == FirstCheckpointId && !gPausePoint.position && !forward) {
    gDebugger._onPause();
    return;
  }
  setPauseState(forward ? PauseModes.RESUMING_FORWARD : PauseModes.RESUMING_BACKWARD,
                gActiveChild.pausePoint(), null);
  finishResume();
  pokeChildren();
}

// Synchronously bring the active child to the specified execution point.
function timeWarp(point) {
  setReplayingPauseTarget(point);
  while (gPauseMode != PauseModes.PAUSED) {
    gActiveChild.waitUntilPaused();
    pokeChildren();
  }
  Services.cpmm.sendAsyncMessage("TimeWarpFinished");
}

////////////////////////////////////////////////////////////////////////////////
// Logpoints
////////////////////////////////////////////////////////////////////////////////

// All installed logpoints. Logpoints are given to us by the debugger, after
// which we need to asynchronously send a child to every point where the
// logpoint's position is reached, evaluate code there and invoke the callback
// associated with the logpoint.
const gLogpoints = [];

// Asynchronously invoke a logpoint's callback with all results from hitting
// the logpoint in the range of the recording covered by checkpoint.
async function findLogpointHits(checkpoint, { position, text, condition, callback }) {
  const hits = await findHits(checkpoint, position);
  const child = ownerChild(checkpoint);
  for (const point of hits) {
    await child.sendManifestAsync({
      contents() {
        return { kind: "hitLogpoint", text, condition };
      },
      onFinished(child, { result }) {
        if (result) {
          callback(point, gDebugger._convertCompletionValue(result));
        }
        child.divergedFromRecording = true;
      },
    }, point);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Saving Recordings
////////////////////////////////////////////////////////////////////////////////

// Resume manifests are sent when the main child is sent forward through the
// recording. Update state according to new data produced by the resume.
function handleResumeManifestResponse({ point, duration, consoleMessages, scripts }) {
  if (!point.position) {
    addCheckpoint(point.checkpoint - 1, duration);
    getCheckpointInfo(point.checkpoint).point = point;
  }

  if (gDebugger && gDebugger.onConsoleMessage) {
    consoleMessages.forEach(msg => gDebugger.onConsoleMessage(msg));
  }

  if (gDebugger && gDebugger.onNewScript) {
    scripts.forEach(script => gDebugger.onNewScript(script));
  }
}

// If necessary, continue executing in the main child.
function maybeResumeRecording() {
  if (gActiveChild != gMainChild) {
    return;
  }

  if (timeSinceCheckpoint(gLastFlushCheckpoint) >= FlushMs) {
    ensureFlushed();
  }

  const checkpoint = gMainChild.pausePoint().checkpoint;
  if (!gMainChild.recording && checkpoint == gRecordingEndpoint) {
    ensureFlushed();
    Services.cpmm.sendAsyncMessage("HitRecordingEndpoint");
    if (gDebugger) {
      gDebugger._onPause();
    }
    return;
  }
  gMainChild.sendManifest({
    contents: { kind: "resume", breakpoints: gBreakpoints },
    onFinished(response) {
      handleResumeManifestResponse(response);

      gPausePoint = gMainChild.pausePoint();
      if (gDebugger) {
        gDebugger._onPause();
      } else {
        Services.tm.dispatchToMainThread(maybeResumeRecording);
      }
    },
  });
}

// If necessary, synchronously flush the recording to disk.
function ensureFlushed() {
  assert(gActiveChild == gMainChild);
  gMainChild.waitUntilPaused(true);

  if (gLastFlushCheckpoint == gActiveChild.pauseCheckpoint()) {
    return;
  }

  if (gMainChild.recording) {
    gMainChild.sendManifest({
      contents: { kind: "flushRecording" },
      onFinished() {},
    });
    gMainChild.waitUntilPaused();
  }

  const oldFlushCheckpoint = gLastFlushCheckpoint || FirstCheckpointId;
  gLastFlushCheckpoint = gMainChild.pauseCheckpoint();

  // We now have a usable recording for replaying children, so spawn them if
  // necessary.
  if (gReplayingChildren.length == 0) {
    spawnReplayingChildren();
  }

  // Checkpoints where the recording was flushed to disk are always saved.
  // This allows the recording to be scanned as soon as it has been flushed.
  addSavedCheckpoint(gLastFlushCheckpoint);

  // Flushing creates a new region of the recording for replaying children
  // to scan.
  forSavedCheckpointsInRange(oldFlushCheckpoint, gLastFlushCheckpoint, checkpoint => {
    scanRecording(checkpoint);

    // Scan for breakpoint and search hits in this new region.
    gBreakpoints.forEach(position => findHits(checkpoint, position));
    gLogpoints.forEach(logpoint => findLogpointHits(checkpoint, logpoint));
  });

  pokeChildren();
}

// eslint-disable-next-line no-unused-vars
function BeforeSaveRecording() {
  if (gActiveChild == gMainChild) {
    // The recording might not be up to date, ensure it flushes after pausing.
    ensureFlushed();
  }
}

// eslint-disable-next-line no-unused-vars
function AfterSaveRecording() {
  Services.cpmm.sendAsyncMessage("SaveRecordingFinished");
}

let gRecordingEndpoint;

function setMainChild() {
  assert(!gMainChild.recording);

  gMainChild.sendManifest({
    contents: { kind: "setMainChild" },
    onFinished({ endpoint }) {
      gRecordingEndpoint = endpoint;
      Services.tm.dispatchToMainThread(maybeResumeRecording);
    },
  });
}

////////////////////////////////////////////////////////////////////////////////
// Child Management
////////////////////////////////////////////////////////////////////////////////

// How many replaying children to spawn. This should be a pref instead...
const NumReplayingChildren = 4;

function spawnReplayingChildren() {
  for (let i = 0; i < NumReplayingChildren; i++) {
    const id = RecordReplayControl.spawnReplayingChild();
    gReplayingChildren[id] = new ChildProcess(id, false);
  }
  addSavedCheckpoint(FirstCheckpointId);
}

// eslint-disable-next-line no-unused-vars
function Initialize(recordingChildId) {
  try {
    if (recordingChildId != undefined) {
      gMainChild = new ChildProcess(recordingChildId, true);
    } else {
      // If there is no recording child, we have now initialized enough state
      // that we can start spawning replaying children.
      const id = RecordReplayControl.spawnReplayingChild();
      gMainChild = new ChildProcess(id, false);
      spawnReplayingChildren();
    }
    gActiveChild = gMainChild;
    return gControl;
  } catch (e) {
    dump(`ERROR: Initialize threw exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function ManifestFinished(id, response) {
  try {
    dumpv(`ManifestFinished #${id} ${JSON.stringify(response)}`);
    lookupChild(id).manifestFinished(response);
  } catch (e) {
    dump(`ERROR: ManifestFinished threw exception: ${e} ${e.stack}\n`);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Debugger Operations
////////////////////////////////////////////////////////////////////////////////

// From the debugger's perspective, there is a single target to interact with,
// represented by gActiveChild. The details of the various children the control
// system is managing are hidden away. This object describes the interface which
// the debugger uses to access the control system.
const gControl = {
  // Get the current point where the active child is paused, or null.
  pausePoint() {
    return gActiveChild && gActiveChild.paused ? gActiveChild.pausePoint() : null;
  },

  // Return whether the active child is currently recording.
  childIsRecording() {
    return gActiveChild && gActiveChild.recording;
  },

  // Ensure the active child is paused.
  waitUntilPaused() {
    // The debugger should not use this method while we are actively resuming.
    assert(gActiveChild);

    if (gActiveChild == gMainChild) {
      gActiveChild.waitUntilPaused(true);
      return;
    }

    while (true) {
      gActiveChild.waitUntilPaused();
      if (pointEquals(gActiveChild.pausePoint(), gPausePoint)) {
        return;
      }
      pokeChild(gActiveChild);
    }
  },

  // Add a breakpoint where the active child should pause while resuming.
  addBreakpoint(position) {
    gBreakpoints.push(position);

    // Start searching for breakpoint hits in the recording immediately.
    if (canFindHits(position)) {
      forSavedCheckpointsInRange(FirstCheckpointId, gLastFlushCheckpoint, checkpoint => {
        findHits(checkpoint, position);
      });
    }

    if (gActiveChild == gMainChild) {
      // The recording child will update its breakpoints when it reaches the
      // next checkpoint, so force it to create a checkpoint now.
      gActiveChild.waitUntilPaused(true);
    }
  },

  // Clear all installed breakpoints.
  clearBreakpoints() {
    gBreakpoints.length = 0;
    if (gActiveChild == gMainChild) {
      // As for addBreakpoint(), update the active breakpoints in the recording
      // child immediately.
      gActiveChild.waitUntilPaused(true);
    }
  },

  // Get the last known point in the recording.
  recordingEndpoint() {
    return gMainChild.lastPausePoint;
  },

  // If the active child is currently recording, switch to a replaying one if
  // possible.
  maybeSwitchToReplayingChild() {
    assert(gActiveChild.paused);
    if (gActiveChild == gMainChild && RecordReplayControl.canRewind()) {
      const point = gActiveChild.pausePoint();

      if (point.position) {
        // We can only flush the recording at checkpoints, so we need to send the
        // main child forward and pause/flush ASAP.
        gMainChild.sendManifest({
          contents: { kind: "resume", breakpoints: [] },
          onFinished(response) {
            handleResumeManifestResponse(response);
          },
        });
        gMainChild.waitUntilPaused(true);
      }

      ensureFlushed();
      pauseReplayingChild(point);
    }
  },

  // Synchronously send a debugger request to a paused active child, returning
  // the response.
  sendRequest(request) {
    let data;
    gActiveChild.sendManifest({
      contents: { kind: "debuggerRequest", request },
      onFinished(finishData) { data = finishData; },
    });
    gActiveChild.waitUntilPaused();

    if (data.restoredCheckpoint) {
      // The child had an unhandled recording diverge and restored an earlier
      // checkpoint. Restore the child to the point it should be paused at and
      // fill its paused state back in by resending earlier debugger requests.
      pauseReplayingChild(gPausePoint);
      gActiveChild.sendManifest({
        contents: { kind: "batchDebuggerRequest", requests: gDebuggerRequests },
        onFinished(finishData) { assert(!finishData.restoredCheckpoint); },
      });
      gActiveChild.waitUntilPaused();
      return { unhandledDivergence: true };
    }

    if (data.divergedFromRecording) {
      // Remember whether the child diverged from the recording.
      gActiveChild.divergedFromRecording = true;
    }

    gDebuggerRequests.push(request);
    return data.response;
  },

  // Synchronously send a debugger request to the main child, which will always
  // be at the end of the recording and can receive requests even when the
  // active child is not currently paused.
  sendRequestMainChild(request) {
    gMainChild.waitUntilPaused(true);
    let data;
    gMainChild.sendManifest({
      contents: { kind: "debuggerRequest", request },
      onFinished(finishData) { data = finishData; },
    });
    gMainChild.waitUntilPaused();
    assert(!data.restoredCheckpoint && !data.divergedFromRecording);
    return data.response;
  },

  resume,
  timeWarp,

  // Add a new logpoint.
  addLogpoint(logpoint) {
    gLogpoints.push(logpoint);
    forSavedCheckpointsInRange(FirstCheckpointId, gLastFlushCheckpoint,
                               checkpoint => findLogpointHits(checkpoint, logpoint));
  },
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

// eslint-disable-next-line no-unused-vars
function ConnectDebugger(dbg) {
  gDebugger = dbg;
  dbg._control = gControl;
}

function dumpv(str) {
  //dump(`[ReplayControl] ${str}\n`);
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function ThrowError(msg)
{
  const error = new Error(msg);
  dump(`ReplayControl Server Error: ${msg} Stack: ${error.stack}\n`);
  throw error;
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "Initialize",
  "ConnectDebugger",
  "ManifestFinished",
  "BeforeSaveRecording",
  "AfterSaveRecording",
];
