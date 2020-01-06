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
const sandbox = Cu.Sandbox(
  CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")()
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "Components.utils.import('resource://gre/modules/Services.jsm');" +
    "Components.utils.import('resource://devtools/shared/execution-point-utils.js');" +
    "Components.utils.import('resource://gre/modules/Timer.jsm');" +
    "addDebuggerToGlobal(this);",
  sandbox
);
const {
  RecordReplayControl,
  Services,
  pointEquals,
  pointToString,
  positionToString,
  findClosestPoint,
  pointArrayIncludes,
  pointPrecedes,
  positionSubsumes,
  setInterval,
} = sandbox;

const InvalidCheckpointId = 0;
const FirstCheckpointId = 1;

// Application State Control
//
// This section describes the strategy used for managing child processes so that
// we can be responsive to user interactions. There is at most one recording
// child process, and any number of replaying child processes.
//
// When the user is not viewing old parts of the recording, they will interact
// with the recording process. The recording process only runs forward and adds
// new data to the recording which the replaying processes can consume.
//
// Replaying processes can be created in two ways: we can spawn a replaying
// process at the beginning of the recording, or we can ask an existing
// replaying process to fork, which creates a new replaying process at the same
// point in the recording.
//
// Replaying processes fit into one of the following categories:
//
// * Root children are spawned at the beginning of the recording. They stop at
//   the first checkpoint and coordinate communication with other processes that
//   have been forked from them.
//
// * Trunk children are forked from the root child. They run through the
//   recording and periodically fork off new branches. There is only one trunk
//   for each root child.
//
// * Branch children are forked from trunk or other branch children. They run
//   forward to a specific point in the recording, and then sit idle while they
//   periodically fork off new leaves.
//
// * Leaf children are forked from branches. They run through a part of the
//   recording and can perform actions such as scanning the recording or
//   performing operations at different points in the recording.
//
// During startup, we will spawn a single trunk child which runs through the
// recording and forks a branch child at certain saved checkpoints. The saved
// checkpoints are spaced so that they aren't far apart. Each branch child forks
// a leaf child which scans the recording from that branch's saved checkpoint up
// to the next saved checkpoint, and then sits idle and responds to queries
// about the scanned region. Other leaf children can be forked from the
// branches, allowing us to quickly perform tasks anywhere in the recording once
// all the branches are in place.
//
// The number of leaves which can simultaneously be operating has a small
// limit to avoid overloading the system. Tasks requiring new children are
// placed in a queue and resolved in priority order.

////////////////////////////////////////////////////////////////////////////////
// Child Processes
////////////////////////////////////////////////////////////////////////////////

// Get a unique ID for a child process.
function processId(rootId, forkId) {
  return forkId ? `#${rootId}:${forkId}` : `#${rootId}`;
}

// Child which is always at the end of the recording. When there is a recording
// child this is it, and when we are replaying an old execution this is a
// replaying child that doesn't fork and is used like a recording child.
let gMainChild;

// All child processes, indexed by their ID.
const gChildren = new Map();

// All child processes that are currently unpaused.
const gUnpausedChildren = new Set();

// Information about a child recording or replaying process.
function ChildProcess(rootId, forkId, recording, recordingLength, startPoint) {
  // ID for the root recording/replaying process this is associated with.
  this.rootId = rootId;

  // ID for the particular fork of the root this process represents, or zero if
  // not forked.
  this.forkId = forkId;

  // Unique ID for this process.
  this.id = processId(rootId, forkId);
  gChildren.set(this.id, this);

  // Whether this process is recording.
  this.recording = recording;

  // How much of the recording is available to this process.
  this.recordingLength = recordingLength;

  // The execution point at which this process was created, or null.
  this.startPoint = startPoint;

  // Whether this process is paused.
  this.paused = false;
  gUnpausedChildren.add(this);

  // The last point we paused at.
  this.lastPausePoint = null;

  // Whether this child has terminated and is unusable.
  this.terminated = false;

  // The last time a ping message was sent to the process.
  this.lastPingTime = Date.now();

  // All pings we have sent since they were reset for this process.
  this.pings = [];

  // Manifests queued up for this child. If the child is unpaused, the first one
  // is currently being processed. Child processes initially have a manifest to
  // run forward to the first checkpoint.
  this.manifests = [
    {
      manifest: { kind: "primordial" },
      onFinished: ({ point, maxRunningProcesses }) => {
        if (this == gMainChild) {
          getCheckpointInfo(FirstCheckpointId).point = point;
          Services.tm.dispatchToMainThread(
            recording ? maybeResumeRecording : setMainChild
          );
        }
        if (maxRunningProcesses) {
          gMaxRunningLeafChildren = maxRunningProcesses;
        }
      },
    },
  ];

  // All manifests that have been sent to this child, used in crash recovery.
  this.processedManifests = [];
}

const PingIntervalSeconds = 2;
const MaxStalledPings = 10;
let gNextPingId = 1;

ChildProcess.prototype = {
  // Stop this child.
  terminate() {
    gChildren.delete(this.id);
    gUnpausedChildren.delete(this);
    RecordReplayControl.terminate(this.rootId, this.forkId);
    this.terminated = true;
  },

  // Take over the process used by another child that was just spawned.
  transplantFrom(newChild) {
    assert(this.terminated && !this.paused);
    assert(!newChild.terminated && !newChild.paused);
    this.terminated = false;
    newChild.terminated = true;
    gUnpausedChildren.delete(newChild);
    gUnpausedChildren.add(this);

    this.rootId = newChild.rootId;
    this.forkId = newChild.forkId;
    this.id = newChild.id;
    gChildren.set(this.id, this);

    this.recordingLength = newChild.recordingLength;
    this.startPoint = newChild.startPoint;
    this.lastPausePoint = newChild.lastPausePoint;
    this.lastPingTime = Date.now();
    this.pings = [];
    this.manifests = newChild.manifests;
    this.processedManifests = newChild.processedManifests;
  },

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
  // executing the manifest, and pauses again when it finishes. This returns a
  // promise that resolves with the manifest's result when it finishes.
  // If specified, onFinishedCallback will be invoked with the manifest
  // result as well.
  sendManifest(manifest, onFinishedCallback) {
    assert(!this.terminated);
    return new Promise(resolve => {
      this.manifests.push({
        manifest,
        onFinished(response) {
          if (onFinishedCallback) {
            onFinishedCallback(response);
          }
          resolve(response);
        },
      });

      if (this.paused) {
        this._startNextManifest();
      }
    });
  },

  // Called when the child's current manifest finishes.
  manifestFinished(response) {
    dumpv(`ManifestFinished ${this.id} ${stringify(response)}`);

    assert(!this.paused);
    this.paused = true;
    gUnpausedChildren.delete(this);
    const { manifest, onFinished } = this.manifests.shift();

    if (response) {
      if (response.point) {
        this.lastPausePoint = response.point;
      }
      if (response.exception) {
        ThrowError(response.exception);
      }
    }
    onFinished(response);

    this.processedManifests.push(manifest);
    this._startNextManifest();
  },

  // Send this child the next manifest in its queue, if there is one.
  _startNextManifest() {
    assert(this.paused);

    if (this.manifests.length) {
      const { manifest } = this.manifests[0];
      dumpv(`SendManifest ${this.id} ${stringify(manifest)}`);
      RecordReplayControl.sendManifest(this.rootId, this.forkId, manifest);
      this.paused = false;
      gUnpausedChildren.add(this);

      // Reset pings when sending a new manifest.
      this.pings.length = 0;
      this.lastPingTime = Date.now();
    }
  },

  // Block until this child is paused. If maybeCreateCheckpoint is specified
  // then a checkpoint is created if this child is recording, so that it pauses
  // quickly (otherwise it could sit indefinitely if there is no page activity).
  waitUntilPaused(maybeCreateCheckpoint) {
    if (this.paused) {
      return;
    }
    dumpv(`WaitUntilPaused ${this.id}`);
    if (maybeCreateCheckpoint) {
      assert(this.recording && !this.forkId);
      RecordReplayControl.createCheckpointInRecording(this.rootId);
    }
    while (!this.paused) {
      this.maybePing();
      RecordReplayControl.maybeProcessNextMessage();
    }
    dumpv(`WaitUntilPaused Done`);
    assert(this.paused || this.terminated);
  },

  // Hang Detection
  //
  // Replaying processes will be terminated if no execution progress has been
  // made for some number of seconds. This generates a crash report for
  // diagnosis and allows another replaying process to be spawned in its place.
  // We detect that no progress is being made by periodically sending ping
  // messages to the replaying process, and comparing the progress values
  // returned by them. The ping messages are sent at least PingIntervalSeconds
  // apart, and the process is considered hanged if at any point the last
  // MaxStalledPings ping messages either did not respond or responded with the
  // same progress value.
  //
  // Dividing our accounting between different ping messages avoids treating
  // processes as hanged when the computer wakes up after sleeping: no pings
  // will be sent while the computer is sleeping and processes are suspended,
  // so the computer will need to be awake for some time before any processes
  // are marked as hanged (before which they will hopefully be able to make
  // progress).
  isHanged() {
    if (this.pings.length < MaxStalledPings) {
      return false;
    }

    const firstIndex = this.pings.length - MaxStalledPings;
    const firstValue = this.pings[firstIndex].progress;
    if (!firstValue) {
      // The child hasn't responded to any of the pings.
      return true;
    }

    for (let i = firstIndex; i < this.pings.length; i++) {
      if (this.pings[i].progress && this.pings[i].progress != firstValue) {
        return false;
      }
    }

    return true;
  },

  maybePing() {
    assert(!this.paused);
    if (this.recording) {
      return;
    }

    const now = Date.now();
    if (now < this.lastPingTime + PingIntervalSeconds * 1000) {
      return;
    }

    if (this.isHanged()) {
      // Try to get the child to crash, so that we can get a minidump.
      RecordReplayControl.crashHangedChild(this.rootId, this.forkId);

      // Treat the child as crashed even if we aren't able to get it to crash.
      ChildCrashed(this.rootId, this.forkId);
    } else {
      const id = gNextPingId++;
      RecordReplayControl.ping(this.rootId, this.forkId, id);
      this.pings.push({ id });
    }
  },

  pingResponse(id, progress) {
    for (const entry of this.pings) {
      if (entry.id == id) {
        entry.progress = progress;
        break;
      }
    }
  },

  updateRecording(length) {
    RecordReplayControl.updateRecording(
      this.rootId,
      this.forkId,
      this.recordingLength,
      length
    );
    this.recordingLength = length;
  },
};

// eslint-disable-next-line no-unused-vars
function ManifestFinished(rootId, forkId, response) {
  try {
    const child = gChildren.get(processId(rootId, forkId));
    if (child) {
      child.manifestFinished(response);
    }
  } catch (e) {
    dump(`ERROR: ManifestFinished threw exception: ${e} ${e.stack}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function PingResponse(rootId, forkId, pingId, progress) {
  try {
    const child = gChildren.get(processId(rootId, forkId));
    if (child) {
      child.pingResponse(pingId, progress);
    }
  } catch (e) {
    dump(`ERROR: PingResponse threw exception: ${e} ${e.stack}\n`);
  }
}

// The singleton ReplayDebugger, or undefined if it doesn't exist.
let gDebugger;

////////////////////////////////////////////////////////////////////////////////
// Child Management
////////////////////////////////////////////////////////////////////////////////

// Priority levels for asynchronous manifests.
const Priority = {
  HIGH: 0,
  MEDIUM: 1,
  LOW: 2,
};

// The singleton root child, if any.
let gRootChild;

// The singleton trunk child, if any.
let gTrunkChild;

// All branch children available for creating new leaves, in order.
const gBranchChildren = [];

// ID for the next fork we create.
let gNextForkId = 1;

// Fork a replaying child, returning the new one.
function forkChild(child, point) {
  const forkId = gNextForkId++;
  const newChild = new ChildProcess(
    child.rootId,
    forkId,
    false,
    child.recordingLength,
    point
  );

  dumpv(`Forking ${child.id} -> ${newChild.id}`);

  child.sendManifest({ kind: "fork", id: forkId });
  return newChild;
}

// Immediately create a new leaf child and take it to endpoint, by forking the
// closest branch child to endpoint. onFinished will be called when the child
// reaches the endpoint.
function newLeafChild(endpoint, onFinished = () => {}) {
  assert(
    !pointPrecedes(checkpointExecutionPoint(gLastSavedCheckpoint), endpoint)
  );

  let entry;
  for (let i = gBranchChildren.length - 1; i >= 0; i--) {
    entry = gBranchChildren[i];
    if (!pointPrecedes(endpoint, entry.point)) {
      break;
    }
  }

  const child = forkChild(entry.child, entry.point);
  if (pointEquals(endpoint, entry.point)) {
    onFinished(child);
  } else {
    child.sendManifest({ kind: "runToPoint", endpoint }, () =>
      onFinished(child)
    );
  }
  return child;
}

// How many leaf children we can have running simultaneously.
let gMaxRunningLeafChildren = 4;

// How many leaf children are currently running.
let gNumRunningLeafChildren = 0;

// Resolve hooks for tasks waiting on a new leaf child, organized by priority.
const gChildWaiters = [[], [], []];

// Asynchronously create a new leaf child and take it to endpoint, respecting
// the limits on the maximum number of running leaves and returning the new
// child when it reaches the endpoint.
async function ensureLeafChild(endpoint, priority = Priority.HIGH) {
  if (gNumRunningLeafChildren < gMaxRunningLeafChildren) {
    gNumRunningLeafChildren++;
  } else {
    await new Promise(resolve => gChildWaiters[priority].push(resolve));
  }

  return new Promise(resolve => {
    newLeafChild(endpoint, child => resolve(child));
  });
}

// After a leaf child becomes idle and/or is about to be terminated, mark it ass
// no longer running and continue any task waiting on a new leaf.
function stopRunningLeafChild() {
  gNumRunningLeafChildren--;

  if (gNumRunningLeafChildren < gMaxRunningLeafChildren) {
    for (const waiters of gChildWaiters) {
      if (waiters.length) {
        const resolve = waiters.shift();
        gNumRunningLeafChildren++;
        resolve();
      }
    }
  }
}

// Terminate a running leaf child that is no longer needed.
function terminateRunningLeafChild(child) {
  stopRunningLeafChild();

  dumpv(`Terminate ${child.id}`);
  child.terminate();
}

// The next ID to use for a root replaying process.
let gNextRootId = 1;

// Spawn the single trunk child.
function spawnTrunkChild() {
  if (!RecordReplayControl.canRewind()) {
    return;
  }

  const id = gNextRootId++;
  RecordReplayControl.spawnReplayingChild(id);
  gRootChild = new ChildProcess(
    id,
    0,
    false,
    RecordReplayControl.recordingLength()
  );

  gTrunkChild = forkChild(
    gRootChild,
    checkpointExecutionPoint(FirstCheckpointId)
  );

  // We should be at the beginning of the recording, and don't have enough space
  // to create any branches yet.
  assert(gLastSavedCheckpoint == InvalidCheckpointId);
}

function forkBranchChild(lastSavedCheckpoint, nextSavedCheckpoint) {
  if (!RecordReplayControl.canRewind()) {
    return;
  }

  // The recording is flushed and the trunk child updated every time we add
  // a saved checkpoint, so the child we fork here will have the recording
  // contents up to the next saved checkpoint. Branch children are only able to
  // run up to the next saved checkpoint.
  gTrunkChild.updateRecording(RecordReplayControl.recordingLength());

  const point = checkpointExecutionPoint(lastSavedCheckpoint);
  const child = forkChild(gTrunkChild, point);

  dumpv(`AddBranchChild Checkpoint ${lastSavedCheckpoint} Child ${child.id}`);
  gBranchChildren.push({ child, point });

  const endpoint = checkpointExecutionPoint(nextSavedCheckpoint);
  gTrunkChild.sendManifest({
    kind: "runToPoint",
    endpoint,

    // External calls are flushed in the trunk child so that external calls
    // from throughout the recording will be cached in the root child.
    flushExternalCalls: true,
  });
}

function respawnCrashedChild(child) {
  const { startPoint, recordingLength, manifests, processedManifests } = child;

  // Find another child to fork that didn't just crash.
  let entry;
  for (let i = gBranchChildren.length - 1; i >= 0; i--) {
    const branch = gBranchChildren[i];
    if (
      !pointPrecedes(child.startPoint, branch.point) &&
      !branch.child.terminated
    ) {
      entry = branch;
      break;
    }
  }
  if (!entry) {
    entry = {
      child: gRootChild,
      point: checkpointExecutionPoint(FirstCheckpointId),
    };
  }

  const newChild = forkChild(entry.child, entry.point);

  dumpv(`Transplanting Child: ${child.id} from ${newChild.id}`);
  child.transplantFrom(newChild);

  if (recordingLength > child.recordingLength) {
    child.updateRecording(recordingLength);
  }

  if (!pointEquals(startPoint, child.startPoint)) {
    assert(pointPrecedes(child.startPoint, startPoint));
    child.sendManifest({ kind: "runToPoint", endpoint: startPoint });
  }

  for (const manifest of processedManifests) {
    if (manifest.kind != "primordial" && manifest.kind != "fork") {
      child.sendManifest(manifest);
    }
  }

  for (const { manifest, onFinished } of manifests) {
    if (
      manifest.kind != "primordial" &&
      (manifest.kind != "fork" || manifest != manifests[0].manifest)
    ) {
      child.sendManifest(manifest, onFinished);
    }
  }
}

// eslint-disable-next-line no-unused-vars
function Initialize(recordingChildId) {
  try {
    if (recordingChildId != undefined) {
      assert(recordingChildId == 0);
      gMainChild = new ChildProcess(recordingChildId, 0, true);
    } else {
      // If there is no recording child, spawn a replaying child in its place.
      const id = gNextRootId++;
      RecordReplayControl.spawnReplayingChild(id);
      gMainChild = new ChildProcess(
        id,
        0,
        false,
        RecordReplayControl.recordingLength
      );
    }
    gActiveChild = gMainChild;
    return gControl;
  } catch (e) {
    dump(`ERROR: Initialize threw exception: ${e}\n`);
  }
}

////////////////////////////////////////////////////////////////////////////////
// PromiseMap
////////////////////////////////////////////////////////////////////////////////

// Map from arbitrary keys to promises that resolve when any associated work is
// complete. This is used to avoid doing redundant work in different async
// tasks.
function PromiseMap() {
  this.map = new Map();
}

PromiseMap.prototype = {
  // Returns either { promise } or { promise, resolve }. If resolve is included
  // then the caller is responsible for invoking it later.
  get(key) {
    let promise = this.map.get(key);
    if (promise) {
      return { promise };
    }

    let resolve;
    promise = new Promise(r => {
      resolve = r;
    });
    this.map.set(key, promise);
    return { promise, resolve };
  },

  set(key, value) {
    this.map.set(key, value);
  },
};

////////////////////////////////////////////////////////////////////////////////
// Application State
////////////////////////////////////////////////////////////////////////////////

// Any child the user is interacting with, which may be paused or not.
let gActiveChild = null;

// Information about each checkpoint, indexed by the checkpoint's id.
const gCheckpoints = [null];

function CheckpointInfo() {
  // The time taken to run from this checkpoint to the next one, excluding idle
  // time.
  this.duration = 0;

  // Execution point at the checkpoint.
  this.point = null;

  // Whether the checkpoint is saved.
  this.saved = false;

  // If the checkpoint is saved, any debugger statement hits in its region.
  this.debuggerStatements = [];

  // If the checkpoint is saved, any events in its region.
  this.events = [];
}

function getCheckpointInfo(id) {
  while (id >= gCheckpoints.length) {
    gCheckpoints.push(new CheckpointInfo());
  }
  return gCheckpoints[id];
}

// How much execution time elapses between two checkpoints.
function checkpointRangeDuration(start, end) {
  let time = 0;
  for (let i = start; i < end; i++) {
    time += gCheckpoints[i].duration;
  }
  return time;
}

// How much execution time has elapsed since a checkpoint.
function timeSinceCheckpoint(id) {
  return checkpointRangeDuration(id, gCheckpoints.length);
}

// How often we want to flush the recording.
const FlushMs = 0.5 * 1000;

// The most recent saved checkpoint. The recording is flushed at every saved
// checkpoint.
let gLastSavedCheckpoint = InvalidCheckpointId;
const gSavedCheckpointWaiters = [];

function addSavedCheckpoint(checkpoint) {
  assert(checkpoint >= gLastSavedCheckpoint);
  if (checkpoint == gLastSavedCheckpoint) {
    return;
  }

  if (gLastSavedCheckpoint != InvalidCheckpointId) {
    forkBranchChild(gLastSavedCheckpoint, checkpoint);
  }

  getCheckpointInfo(checkpoint).saved = true;
  gLastSavedCheckpoint = checkpoint;

  gSavedCheckpointWaiters.forEach(resolve => resolve());
  gSavedCheckpointWaiters.length = 0;
}

function waitForSavedCheckpoint() {
  return new Promise(resolve => gSavedCheckpointWaiters.push(resolve));
}

function addCheckpoint(checkpoint, duration) {
  assert(!getCheckpointInfo(checkpoint).duration);
  getCheckpointInfo(checkpoint).duration = duration;
}

function nextSavedCheckpoint(checkpoint) {
  assert(checkpoint == InvalidCheckpointId || gCheckpoints[checkpoint].saved);
  while (++checkpoint < gCheckpoints.length) {
    if (gCheckpoints[checkpoint].saved) {
      return checkpoint;
    }
  }
  return undefined;
}

function forSavedCheckpointsInRange(start, end, callback) {
  if (start == FirstCheckpointId && !gCheckpoints[start].saved) {
    return;
  }
  assert(gCheckpoints[start].saved);
  for (
    let checkpoint = start;
    checkpoint < end;
    checkpoint = nextSavedCheckpoint(checkpoint)
  ) {
    callback(checkpoint);
  }
}

function forAllSavedCheckpoints(callback) {
  forSavedCheckpointsInRange(FirstCheckpointId, gLastSavedCheckpoint, callback);
}

function getSavedCheckpoint(checkpoint) {
  while (!gCheckpoints[checkpoint].saved) {
    checkpoint--;
  }
  return checkpoint;
}

// Get the execution point to use for a checkpoint.
function checkpointExecutionPoint(checkpoint) {
  return gCheckpoints[checkpoint].point;
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
// by sending a manifest to the child which performed the scan. Scanning is done
// using leaf children. When the child has finished scanning, it is marked as no
// longer running and remains idle remains idle while it responds to queries on
// the scanned data.

// Map from saved checkpoints to the leaf child responsible for scanning that saved
// checkpoint's region.
const gSavedCheckpointChildren = new PromiseMap();

// Set of saved checkpoints which have finished scanning.
const gScannedSavedCheckpoints = new Set();

// Ensure the region for a saved checkpoint has been scanned by some child, and
// return that child.
async function scanRecording(checkpoint) {
  assert(gCheckpoints[checkpoint].saved);
  if (checkpoint == gLastSavedCheckpoint) {
    await waitForSavedCheckpoint();
  }

  const { promise, resolve } = gSavedCheckpointChildren.get(checkpoint);
  if (!resolve) {
    return promise;
  }

  const endpoint = checkpointExecutionPoint(nextSavedCheckpoint(checkpoint));
  const child = await ensureLeafChild(checkpointExecutionPoint(checkpoint));
  await child.sendManifest({ kind: "scanRecording", endpoint });

  stopRunningLeafChild();

  gScannedSavedCheckpoints.add(checkpoint);

  // Update the unscanned regions in the UI.
  if (gDebugger) {
    gDebugger._callOnPositionChange();
  }

  resolve(child);
  return child;
}

function unscannedRegions() {
  const result = [];

  function addRegion(startCheckpoint, endCheckpoint) {
    const start = checkpointExecutionPoint(startCheckpoint).progress;
    const end = checkpointExecutionPoint(endCheckpoint).progress;

    if (result.length && result[result.length - 1].end == start) {
      result[result.length - 1].end = end;
    } else {
      result.push({ start, end });
    }
  }

  forAllSavedCheckpoints(checkpoint => {
    if (!gScannedSavedCheckpoints.has(checkpoint)) {
      addRegion(checkpoint, nextSavedCheckpoint(checkpoint));
    }
  });

  const lastFlush = gLastSavedCheckpoint || FirstCheckpointId;
  if (lastFlush != gRecordingEndpoint) {
    addRegion(lastFlush, gMainChild.lastPausePoint.checkpoint);
  }

  return result;
}

// Map from saved checkpoints and positions to the breakpoint hits for that
// position within the range of the checkpoint.
const gHitSearches = new PromiseMap();

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
  assert(gCheckpoints[checkpoint].saved);

  const key = `${checkpoint}:${positionToString(position)}`;
  const { promise, resolve } = gHitSearches.get(key);
  if (!resolve) {
    return promise;
  }

  const endpoint = nextSavedCheckpoint(checkpoint);
  const child = await scanRecording(checkpoint);
  const hits = await child.sendManifest({
    kind: "findHits",
    position,
    startpoint: checkpoint,
    endpoint,
  });

  resolve(hits);
  return hits;
}

// Asynchronously find all hits on a breakpoint's position.
async function findBreakpointHits(checkpoint, position) {
  if (position.kind == "Break") {
    findHits(checkpoint, position);
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

// Map from point strings to the steps which contain them.
const gFrameSteps = new PromiseMap();

// Map from frame entry point strings to the parent frame's entry point.
const gParentFrames = new PromiseMap();

// Find all the steps in the frame which point is part of. This returns a
// promise that resolves with the steps that were found.
async function findFrameSteps(point) {
  if (!point.position) {
    return null;
  }

  assert(
    point.position.kind == "EnterFrame" ||
      point.position.kind == "OnStep" ||
      point.position.kind == "OnPop"
  );

  const { promise, resolve } = gFrameSteps.get(pointToString(point));
  if (!resolve) {
    return promise;
  }

  // Gather information which the child which did the scan can use to figure out
  // the different frame steps.
  const info = gControl.sendRequestMainChild({
    type: "frameStepsInfo",
    script: point.position.script,
  });

  const checkpoint = getSavedCheckpoint(point.checkpoint);
  const child = await scanRecording(checkpoint);
  const steps = await child.sendManifest({
    kind: "findFrameSteps",
    targetPoint: point,
    ...info,
  });

  for (const p of steps) {
    if (p.position.frameIndex == point.position.frameIndex) {
      gFrameSteps.set(pointToString(p), steps);
    } else {
      assert(p.position.kind == "EnterFrame");
      gParentFrames.set(pointToString(p), steps[0]);
    }
  }

  resolve(steps);
  return steps;
}

async function findParentFrameEntryPoint(point) {
  assert(point.position.kind == "EnterFrame");
  assert(point.position.frameIndex > 0);

  const { promise, resolve } = gParentFrames.get(pointToString(point));
  if (!resolve) {
    return promise;
  }

  const checkpoint = getSavedCheckpoint(point.checkpoint);
  const child = await scanRecording(checkpoint);
  const { parentPoint } = await child.sendManifest({
    kind: "findParentFrameEntryPoint",
    point,
  });

  resolve(parentPoint);
  return parentPoint;
}

////////////////////////////////////////////////////////////////////////////////
// Pause Data
////////////////////////////////////////////////////////////////////////////////

const gPauseData = new Map();
const gQueuedPauseData = new Set();

// Cached points indicate messages where we have gathered pause data. These are
// shown differently in the UI.
const gCachedPoints = new Map();

async function queuePauseData({ point, trackCached }) {
  if (gQueuedPauseData.has(pointToString(point))) {
    return;
  }
  gQueuedPauseData.add(pointToString(point));

  await waitForFlushed(point.checkpoint);

  const child = await ensureLeafChild(point, Priority.LOW);
  const data = await child.sendManifest({ kind: "getPauseData" });

  addPauseData(point, data, trackCached);
  terminateRunningLeafChild(child);
}

function addPauseData(point, data, trackCached) {
  if (data.paintData) {
    // Atomize paint data strings to ensure that we don't store redundant
    // strings for execution points with the same paint data.
    data.paintData = RecordReplayControl.atomize(data.paintData);
  }
  gPauseData.set(pointToString(point), data);

  if (trackCached) {
    gCachedPoints.set(pointToString(point), point);
    if (gDebugger) {
      gDebugger._callOnPositionChange();
    }
  }
}

function maybeGetPauseData(point) {
  return gPauseData.get(pointToString(point));
}

function cachedPoints() {
  return [...gCachedPoints.values()].map(point => point.progress);
}

////////////////////////////////////////////////////////////////////////////////
// Pause State
////////////////////////////////////////////////////////////////////////////////

// The pause mode classifies the current state of the debugger.
const PauseModes = {
  // The main child is the active child, and is either paused or actively
  // recording. gPausePoint is the last point the main child reached.
  RUNNING: "RUNNING",

  // gActiveChild is a replaying child paused or being taken to gPausePoint.
  PAUSED: "PAUSED",

  // gActiveChild is null, and we are looking for the last breakpoint hit prior
  // to or following gPausePoint, at which we will pause.
  RESUMING_BACKWARD: "RESUMING_BACKWARD",
  RESUMING_FORWARD: "RESUMING_FORWARD",
};

// Current pause mode.
let gPauseMode = PauseModes.RUNNING;

// In PAUSED mode, the point we are paused at or sending the active child to.
let gPausePoint = null;

// In PAUSED mode, any debugger requests that have been sent to the child.
const gDebuggerRequests = [];

function addDebuggerRequest(request) {
  gDebuggerRequests.push({
    request,
    stack: Error().stack,
  });
}

function setPauseState(mode, point, child) {
  assert(mode);
  const idString = child ? ` ${child.id}` : "";
  dumpv(`SetPauseState ${mode} ${JSON.stringify(point)}${idString}`);

  if (gActiveChild && gActiveChild != gMainChild && gActiveChild != child) {
    terminateRunningLeafChild(gActiveChild);
  }

  gPauseMode = mode;
  gPausePoint = point;
  gActiveChild = child;

  if (mode == PauseModes.PAUSED) {
    simulateNearbyNavigation();
  }
}

// Mark the debugger as paused, and asynchronously send a child to the pause
// point.
function setReplayingPauseTarget(point) {
  assert(!gDebuggerRequests.length);

  const child = newLeafChild(point);
  setPauseState(PauseModes.PAUSED, point, child);

  gDebugger._onPause();
  findFrameSteps(point);
}

// Synchronously bring a new leaf child to the current pause point.
function bringNewReplayingChildToPausePoint() {
  const child = newLeafChild(gPausePoint);
  setPauseState(PauseModes.PAUSED, gPausePoint, child);

  child.sendManifest({
    kind: "batchDebuggerRequest",
    requests: gDebuggerRequests.map(r => r.request),
  });
}

// Find the point where the debugger should pause when running forward or
// backward from a point and using a given set of breakpoints. Returns null if
// there is no point to pause at before hitting the beginning or end of the
// recording.
async function resumeTarget(point, forward, breakpoints) {
  let startCheckpoint = point.checkpoint;
  if (!forward && !point.position) {
    startCheckpoint--;
    if (startCheckpoint == InvalidCheckpointId) {
      return null;
    }
  }
  startCheckpoint = getSavedCheckpoint(startCheckpoint);

  let checkpoint = startCheckpoint;
  for (; ; forward ? checkpoint++ : checkpoint--) {
    if ([InvalidCheckpointId, gLastSavedCheckpoint].includes(checkpoint)) {
      return null;
    }

    if (!gCheckpoints[checkpoint].saved) {
      continue;
    }

    const hits = [];

    // Find any breakpoint hits in this region of the recording.
    for (const bp of breakpoints) {
      if (canFindHits(bp)) {
        const bphits = await findHits(checkpoint, bp);
        hits.push(...bphits);
      }
    }

    // When there are stepping breakpoints, look for breakpoint hits in the
    // steps for the current frame.
    if (
      checkpoint == startCheckpoint &&
      gBreakpoints.some(bp => bp.kind == "EnterFrame" || bp.kind == "OnPop")
    ) {
      const steps = await findFrameSteps(point);
      hits.push(
        ...steps.filter(p => {
          return breakpoints.some(bp => positionSubsumes(bp, p.position));
        })
      );
    }

    // Always pause at debugger statements, as if they are breakpoint hits.
    hits.push(...getCheckpointInfo(checkpoint).debuggerStatements);

    const hit = findClosestPoint(
      hits,
      gPausePoint,
      /* before */ !forward,
      /* inclusive */ false
    );
    if (hit) {
      return hit;
    }
  }
}

async function finishResume() {
  assert(
    gPauseMode == PauseModes.RESUMING_FORWARD ||
      gPauseMode == PauseModes.RESUMING_BACKWARD
  );
  const forward = gPauseMode == PauseModes.RESUMING_FORWARD;

  const point = await resumeTarget(gPausePoint, forward, gBreakpoints);
  if (point) {
    // We found a point to pause at.
    setReplayingPauseTarget(point);
  } else if (forward) {
    // We searched the entire space forward to the end of the recording and
    // didn't find any breakpoint hits, so resume recording.
    assert(forward);
    RecordReplayControl.restoreMainGraphics();
    setPauseState(PauseModes.RUNNING, gMainChild.pausePoint(), gMainChild);
    gDebugger._callOnPositionChange();
    maybeResumeRecording();
  } else {
    // We searched backward to the beginning of the recording, so restore the
    // first checkpoint.
    assert(!forward);
    setReplayingPauseTarget(checkpointExecutionPoint(FirstCheckpointId));
  }
}

// Unpause the active child and asynchronously pause at the next or previous
// breakpoint hit.
function resume(forward) {
  gDebuggerRequests.length = 0;
  if (gActiveChild.recording) {
    if (forward) {
      maybeResumeRecording();
      return;
    }
    ensureFlushed();
  }
  if (
    gPausePoint.checkpoint == FirstCheckpointId &&
    !gPausePoint.position &&
    !forward
  ) {
    gDebugger._hitRecordingBoundary();
    return;
  }
  setPauseState(
    forward ? PauseModes.RESUMING_FORWARD : PauseModes.RESUMING_BACKWARD,
    gPausePoint,
    null
  );
  finishResume();
}

// Synchronously bring the active child to the specified execution point.
function timeWarp(point) {
  gDebuggerRequests.length = 0;
  setReplayingPauseTarget(point);
  Services.cpmm.sendAsyncMessage("TimeWarpFinished");
}

////////////////////////////////////////////////////////////////////////////////
// Crash Recovery
////////////////////////////////////////////////////////////////////////////////

// The maximum number of crashes which we can recover from.
const MaxCrashes = 4;

// How many child processes have crashed.
let gNumCrashes = 0;

// eslint-disable-next-line no-unused-vars
function ChildCrashed(rootId, forkId) {
  const id = processId(rootId, forkId);
  dumpv(`Child Crashed: ${id}`);

  const child = gChildren.get(id);
  if (!child) {
    return;
  }

  // Crashes in recording children can't be recovered.
  if (child.recording) {
    ThrowError("Child is recording");
  }

  // Crashes in root processes can't be recovered, as all communication to forks
  // happens through the root.
  if (!forkId) {
    ThrowError("Child is replaying root");
  }

  // Children shouldn't be crashing if they aren't doing anything.
  if (child.paused) {
    ThrowError("Child is paused");
  }

  if (++gNumCrashes > MaxCrashes) {
    ThrowError("Too many crashes");
  }

  child.terminate();

  // If the child crashed while forking, the forked child may or may not have
  // been created. Mark the fork as crashed so that a new process will be
  // generated in its place (and any successfully forked process ignored).
  const { manifest } = child.manifests[0];
  if (manifest.kind == "fork") {
    ChildCrashed(rootId, manifest.id);
  }

  respawnCrashedChild(child);
}

////////////////////////////////////////////////////////////////////////////////
// Simulated Navigation
////////////////////////////////////////////////////////////////////////////////

// When the user is paused somewhere in the recording, we want to obtain pause
// data for points which they can get to via the UI. This includes all messages
// on the timeline (including those for logpoints), breakpoints that can be
// reached by rewinding and resuming, and points that can be reached by
// stepping. In the latter two cases, we only want to queue up the pause data
// for points that are close to the current pause point, so that we don't waste
// time and resources getting pause data that isn't immediately needed.
// We handle the latter by simulating the results of user interactions to
// visit nearby breakpoints and steps, and queueing up the pause data for those
// points. The simulation is approximate, as stepping behavior in particular
// depends on information known to the thread actor which isn't available here.
//
// When the user navigates through the recording, these simulations are repeated
// to queue up pause data at new points. Ideally, we will capture the pause data
// quickly enough that it will be immediately available when the user pauses at
// a new location, so that they don't experience loading hiccups.

// Get the pause data for nearby breakpoint hits, ignoring steps.
async function simulateBreakpointNavigation(point, forward, count) {
  if (!count) {
    return;
  }

  const breakpoints = gBreakpoints.filter(bp => bp.kind == "Break");
  const next = await resumeTarget(point, forward, breakpoints);
  if (next) {
    queuePauseData({ point: next });
    simulateBreakpointNavigation(next, forward, count - 1);
  }
}

async function findFrameEntryPoint(point) {
  // We never want the debugger to stop at EnterFrame points corresponding to
  // when a frame was pushed on the stack. Instead, find the first point in the
  // frame which is a breakpoint site.
  assert(point.position.kind == "EnterFrame");
  const steps = await findFrameSteps(point);
  assert(pointEquals(steps[0], point));
  return steps[1];
}

// eslint-disable-next-line complexity
async function simulateSteppingNavigation(point, count, frameCount, last) {
  if (!count || !point.position) {
    return;
  }
  const { script } = point.position;
  const dbgScript = gDebugger._getScript(script);

  const steps = await findFrameSteps(point);
  const pointIndex = steps.findIndex(p => pointEquals(p, point));

  if (last != "reverseStepOver") {
    for (let i = pointIndex + 1; i < steps.length; i++) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        queuePauseData({ point: p, snapshot: steps[0] });
        simulateSteppingNavigation(p, count - 1, frameCount, "stepOver");
        break;
      }
    }
  }

  if (last != "stepOver") {
    for (let i = pointIndex - 1; i >= 1; i--) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        queuePauseData({ point: p, snapshot: steps[0] });
        simulateSteppingNavigation(p, count - 1, frameCount, "reverseStepOver");
        break;
      }
    }
  }

  if (frameCount) {
    for (let i = pointIndex + 1; i < steps.length; i++) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        break;
      }
      if (p.position.script != script) {
        // Currently, the debugger will stop at the EnterFrame site and then run
        // forward to the first breakpoint site before pausing. We need pause
        // data from both points, unfortunately.
        queuePauseData({ point: p, snapshot: steps[0] });

        const np = await findFrameEntryPoint(p);
        queuePauseData({ point: np, snapshot: steps[0] });
        if (canFindHits(np.position)) {
          findHits(getSavedCheckpoint(point.checkpoint), np.position);
        }
        simulateSteppingNavigation(np, count - 1, frameCount - 1, "stepIn");
        break;
      }
    }
  }

  if (
    frameCount &&
    last != "stepOver" &&
    last != "reverseStepOver" &&
    point.position.frameIndex
  ) {
    // The debugger will stop at the OnPop for the frame before finding a place
    // in the parent frame to pause at.
    queuePauseData({ point: steps[steps.length - 1], snapshot: steps[0] });

    const parentEntryPoint = await findParentFrameEntryPoint(steps[0]);
    const parentSteps = await findFrameSteps(parentEntryPoint);
    for (let i = 0; i < parentSteps.length; i++) {
      const p = parentSteps[i];
      if (pointPrecedes(point, p)) {
        // When stepping out we will stop at the next breakpoint site,
        // and do not need a point that is a stepping target.
        queuePauseData({ point: p, snapshot: parentSteps[0] });
        if (canFindHits(p.position)) {
          findHits(getSavedCheckpoint(point.checkpoint), p.position);
        }
        simulateSteppingNavigation(p, count - 1, frameCount - 1, "stepOut");
        break;
      }
    }
  }

  function isStepOverTarget(p) {
    const { kind, offset } = p.position;
    return (
      kind == "OnPop" ||
      (kind == "OnStep" && dbgScript.getOffsetMetadata(offset).isStepStart)
    );
  }
}

function simulateNearbyNavigation() {
  // How many breakpoint hit navigations are simulated on either side of the
  // pause point.
  const numBreakpointHits = 2;

  // Maximum number of steps to take when simulating nearby steps.
  const numSteps = 4;

  // Maximum number of times to change frames when simulating nearby steps.
  const numChangeFrames = 2;

  simulateBreakpointNavigation(gPausePoint, true, numBreakpointHits);
  simulateBreakpointNavigation(gPausePoint, false, numBreakpointHits);
  simulateSteppingNavigation(gPausePoint, numSteps, numChangeFrames);
}

////////////////////////////////////////////////////////////////////////////////
// Logpoints
////////////////////////////////////////////////////////////////////////////////

// All installed logpoints. Logpoints are given to us by the debugger, after
// which we need to asynchronously send a child to every point where the
// logpoint's position is reached, evaluate code there and invoke the callback
// associated with the logpoint.
const gLogpoints = [];

async function evaluateLogpoint({
  point,
  text,
  condition,
  callback,
  snapshot,
  fast,
}) {
  assert(point);

  const child = await ensureLeafChild(point, Priority.MEDIUM);
  const { result, resultData } = await child.sendManifest({
    kind: "hitLogpoint",
    text,
    condition,
    fast,
  });
  terminateRunningLeafChild(child);

  if (result) {
    callback(point, result, resultData);
  }
}

// Asynchronously invoke a logpoint's callback with all results from hitting
// the logpoint in the range of the recording covered by checkpoint.
async function findLogpointHits(
  checkpoint,
  { position, text, condition, messageCallback, validCallback }
) {
  if (!validCallback()) {
    return;
  }

  const hits = await findHits(checkpoint, position);
  hits.sort((a, b) => pointPrecedes(b, a));

  // Show an initial message while we evaluate the logpoint at each point.
  if (!condition) {
    for (const point of hits) {
      messageCallback(point, ["Loading..."]);
    }
  }

  for (const point of hits) {
    // When the fast logpoints mode is set, replaying children do not diverge
    // from the recording when evaluating logpoints. If the evaluation has side
    // effects then the page can behave differently later, including crashes if
    // the recording is perturbed. Caveat emptor!
    const fast = getPreference("fastLogpoints");

    // Create a snapshot at the most recent checkpoint in case there are other
    // nearby logpoints, except in fast mode where there is no need to rewind.
    const snapshot = !fast && checkpointExecutionPoint(point.checkpoint);

    // We wait on each logpoint in the list before starting the next one, so
    // that hopefully all the logpoints associated with the same save checkpoint
    // will be handled by the same process, with no duplicated work due to
    // different processes handling logpoints that are very close to each other.
    await evaluateLogpoint({
      point,
      text,
      condition,
      callback: messageCallback,
      snapshot,
      fast,
    });

    if (!validCallback()) {
      return;
    }
  }

  // Now that we've done the evaluation, gather pause data for each point we
  // logged. We do this in a second pass because we want to do the evaluations
  // as quickly as possible.
  for (const point of hits) {
    await queuePauseData({ point, trackCached: true });

    if (!validCallback()) {
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Event Breakpoints
////////////////////////////////////////////////////////////////////////////////

// Event kinds which will be logged. For now this set can only grow, as we don't
// have a way to remove old event logpoints from the client.
const gLoggedEvents = [];

const gEventFrameEntryPoints = new PromiseMap();

async function findEventFrameEntry(checkpoint, progress) {
  const { promise, resolve } = gEventFrameEntryPoints.get(progress);
  if (!resolve) {
    return promise;
  }

  const savedCheckpoint = getSavedCheckpoint(checkpoint);
  const child = await scanRecording(savedCheckpoint);
  const { rv } = await child.sendManifest({
    kind: "findEventFrameEntry",
    checkpoint,
    progress,
  });

  const point = await findFrameEntryPoint(rv);
  resolve(point);
  return point;
}

async function findEventLogpointHits(checkpoint, event, callback) {
  for (const info of getCheckpointInfo(checkpoint).events) {
    if (info.event == event) {
      const point = await findEventFrameEntry(info.checkpoint, info.progress);
      if (point) {
        callback(point, ["Loading..."]);
        evaluateLogpoint({ point, text: "arguments[0]", callback });
        queuePauseData({ point, trackCached: true });
      }
    }
  }
}

function setActiveEventBreakpoints(events, callback) {
  dumpv(`SetActiveEventBreakpoints ${JSON.stringify(events)}`);

  for (const event of events) {
    if (gLoggedEvents.some(info => info.event == event)) {
      continue;
    }
    gLoggedEvents.push({ event, callback });
    forAllSavedCheckpoints(checkpoint =>
      findEventLogpointHits(checkpoint, event, callback)
    );
  }
}

////////////////////////////////////////////////////////////////////////////////
// Saving Recordings
////////////////////////////////////////////////////////////////////////////////

// Resume manifests are sent when the main child is sent forward through the
// recording. Update state according to new data produced by the resume.
function handleResumeManifestResponse({
  point,
  duration,
  consoleMessages,
  scripts,
  debuggerStatements,
  events,
}) {
  if (!point.position) {
    addCheckpoint(point.checkpoint - 1, duration);
    getCheckpointInfo(point.checkpoint).point = point;
  }

  if (gDebugger) {
    consoleMessages.forEach(msg => {
      gDebugger._newConsoleMessage(msg);
    });
  }

  if (gDebugger) {
    scripts.forEach(script => gDebugger._onNewScript(script));
  }

  consoleMessages.forEach(msg => {
    if (msg.executionPoint) {
      queuePauseData({ point: msg.executionPoint, trackCached: true });
    }
  });

  const savedCheckpoint = getSavedCheckpoint(
    point.position ? point.checkpoint : point.checkpoint - 1
  );

  for (const point of debuggerStatements) {
    getCheckpointInfo(savedCheckpoint).debuggerStatements.push(point);
  }

  for (const event of events) {
    getCheckpointInfo(savedCheckpoint).events.push(event);
  }

  // In repaint stress mode, the child process creates a checkpoint before every
  // paint. By gathering the pause data at these checkpoints, we will perform a
  // repaint at all of these checkpoints, ensuring that all the normal paints
  // can be repainted.
  if (RecordReplayControl.inRepaintStressMode()) {
    queuePauseData({ point });
  }
}

// If necessary, continue executing in the main child.
function maybeResumeRecording() {
  if (gActiveChild != gMainChild) {
    return;
  }

  if (
    !gLastSavedCheckpoint ||
    timeSinceCheckpoint(gLastSavedCheckpoint) >= FlushMs
  ) {
    ensureFlushed();
  }

  const checkpoint = gMainChild.pausePoint().checkpoint;
  if (!gMainChild.recording && checkpoint == gRecordingEndpoint) {
    ensureFlushed();
    Services.cpmm.sendAsyncMessage("HitRecordingEndpoint");
    if (gDebugger) {
      gDebugger._hitRecordingBoundary();
    }
    return;
  }
  gMainChild.sendManifest(
    {
      kind: "resume",
      breakpoints: gBreakpoints,
      pauseOnDebuggerStatement: !!gDebugger,
    },
    response => {
      handleResumeManifestResponse(response);

      gPausePoint = gMainChild.pausePoint();
      if (gDebugger) {
        gDebugger._onPause();
      } else {
        Services.tm.dispatchToMainThread(maybeResumeRecording);
      }
    }
  );
}

// Resolve callbacks for any promises waiting on the recording to be flushed.
const gFlushWaiters = [];

function waitForFlushed(checkpoint) {
  if (checkpoint < gLastSavedCheckpoint) {
    return undefined;
  }
  return new Promise(resolve => {
    gFlushWaiters.push(resolve);
  });
}

let gLastFlushTime = Date.now();

// If necessary, synchronously flush the recording to disk.
function ensureFlushed() {
  gMainChild.waitUntilPaused(true);

  gLastFlushTime = Date.now();

  if (gLastSavedCheckpoint == gMainChild.pauseCheckpoint()) {
    return;
  }

  if (gMainChild.recording) {
    gMainChild.sendManifest({ kind: "flushRecording" });
    gMainChild.waitUntilPaused();
  }

  // We now have a usable recording for replaying children, so spawn them if
  // necessary.
  if (!gTrunkChild) {
    spawnTrunkChild();
  }

  const oldSavedCheckpoint = gLastSavedCheckpoint || FirstCheckpointId;
  addSavedCheckpoint(gMainChild.pauseCheckpoint());

  // Checkpoints where the recording was flushed to disk are saved. This allows
  // the recording to be scanned as soon as it has been flushed.

  // Flushing creates a new region of the recording for replaying children
  // to scan.
  forSavedCheckpointsInRange(
    oldSavedCheckpoint,
    gLastSavedCheckpoint,
    checkpoint => {
      scanRecording(checkpoint);

      // Scan for breakpoint and logpoint hits in this new region.
      gBreakpoints.forEach(position =>
        findBreakpointHits(checkpoint, position)
      );
      gLogpoints.forEach(logpoint => findLogpointHits(checkpoint, logpoint));
      for (const { event, callback } of gLoggedEvents) {
        findEventLogpointHits(checkpoint, event, callback);
      }
    }
  );

  for (const waiter of gFlushWaiters) {
    waiter();
  }
  gFlushWaiters.length = 0;
}

const CheckFlushMs = 1000;

setInterval(() => {
  // Periodically make sure the recording is flushed. If the tab is sitting
  // idle we still want to keep the recording up to date.
  const elapsed = Date.now() - gLastFlushTime;
  if (
    elapsed > CheckFlushMs &&
    gMainChild.lastPausePoint &&
    gMainChild.lastPausePoint.checkpoint != gLastSavedCheckpoint
  ) {
    ensureFlushed();
  }

  // Ping children that are executing manifests to ensure they haven't hanged.
  for (const child of gUnpausedChildren) {
    if (!child.recording) {
      child.maybePing();
    }
  }
}, 1000);

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

async function setMainChild() {
  assert(!gMainChild.recording);

  const { endpoint } = await gMainChild.sendManifest({ kind: "setMainChild" });
  gRecordingEndpoint = endpoint;
  Services.tm.dispatchToMainThread(maybeResumeRecording);
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
    if (gActiveChild && gActiveChild == gMainChild) {
      return gActiveChild.paused ? gActiveChild.pausePoint() : null;
    }
    if (gPauseMode == PauseModes.PAUSED) {
      return gPausePoint;
    }
    return null;
  },

  lastPausePoint() {
    return gPausePoint;
  },

  findFrameSteps(point) {
    return findFrameSteps(point);
  },

  async findAncestorFrameEntryPoint(point, index) {
    const steps = await findFrameSteps(point);
    point = steps[0];
    while (index--) {
      point = await findParentFrameEntryPoint(point);
    }
    return point;
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
    }
  },

  // Add a breakpoint where the active child should pause while resuming.
  addBreakpoint(position) {
    dumpv(`AddBreakpoint ${JSON.stringify(position)}`);
    gBreakpoints.push(position);

    // Start searching for breakpoint hits in the recording immediately.
    if (canFindHits(position)) {
      forAllSavedCheckpoints(checkpoint =>
        findBreakpointHits(checkpoint, position)
      );
    }

    if (gActiveChild == gMainChild) {
      // The recording child will update its breakpoints when it reaches the
      // next checkpoint, so force it to create a checkpoint now.
      gActiveChild.waitUntilPaused(true);
    }

    simulateNearbyNavigation();
  },

  // Clear all installed breakpoints.
  clearBreakpoints() {
    dumpv(`ClearBreakpoints`);
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
    assert(gControl.pausePoint());
    if (gActiveChild == gMainChild && RecordReplayControl.canRewind()) {
      const point = gActiveChild.pausePoint();

      if (point.position) {
        // We can only flush the recording at checkpoints, so we need to send the
        // main child forward and pause/flush ASAP.
        gMainChild.sendManifest(
          {
            kind: "resume",
            breakpoints: [],
            pauseOnDebuggerStatement: false,
          },
          handleResumeManifestResponse
        );
        gMainChild.waitUntilPaused(true);
      }

      ensureFlushed();
      bringNewReplayingChildToPausePoint();
    }
  },

  // Synchronously send a debugger request to a paused active child, returning
  // the response.
  sendRequest(request) {
    let data;
    gActiveChild.sendManifest(
      { kind: "debuggerRequest", request },
      finishData => {
        data = finishData;
      }
    );
    while (!data) {
      gActiveChild.waitUntilPaused();
    }

    if (data.unhandledDivergence) {
      bringNewReplayingChildToPausePoint();
    } else {
      addDebuggerRequest(request);
    }
    return data.response;
  },

  // Synchronously send a debugger request to the main child, which will always
  // be at the end of the recording and can receive requests even when the
  // active child is not currently paused.
  sendRequestMainChild(request) {
    gMainChild.waitUntilPaused(true);
    let data;
    gMainChild.sendManifest(
      { kind: "debuggerRequest", request },
      finishData => {
        data = finishData;
      }
    );
    gMainChild.waitUntilPaused();
    assert(!data.divergedFromRecording);
    return data.response;
  },

  resume,
  timeWarp,

  // Add a new logpoint.
  addLogpoint(logpoint) {
    gLogpoints.push(logpoint);
    forAllSavedCheckpoints(checkpoint =>
      findLogpointHits(checkpoint, logpoint)
    );
  },

  setActiveEventBreakpoints,

  unscannedRegions,
  cachedPoints,

  debuggerRequests() {
    return gDebuggerRequests;
  },

  getPauseDataAndRepaint() {
    // Use cached pause data if possible, which we can immediately return
    // without waiting for the child to arrive at the pause point.
    if (!gDebuggerRequests.length) {
      const data = maybeGetPauseData(gPausePoint);
      if (data) {
        // After the child pauses, it will need to generate the pause data so
        // that any referenced objects will be instantiated.
        gActiveChild.sendManifest({
          kind: "debuggerRequest",
          request: { type: "pauseData" },
        });
        addDebuggerRequest({ type: "pauseData" });
        RecordReplayControl.hadRepaint(data.paintData);
        return data;
      }
    }
    gControl.maybeSwitchToReplayingChild();
    const data = gControl.sendRequest({ type: "pauseData" });
    if (!data) {
      RecordReplayControl.clearGraphics();
    } else {
      addPauseData(gPausePoint, data, /* trackCached */ true);
      if (data.paintData) {
        RecordReplayControl.hadRepaint(data.paintData);
      }
    }
    return data;
  },

  paint(point) {
    const data = maybeGetPauseData(point);
    if (data && data.paintData) {
      RecordReplayControl.hadRepaint(data.paintData);
    }
  },

  isPausedAtDebuggerStatement() {
    const point = gControl.pausePoint();
    if (point) {
      const checkpoint = getSavedCheckpoint(point.checkpoint);
      const { debuggerStatements } = getCheckpointInfo(checkpoint);
      return pointArrayIncludes(debuggerStatements, point);
    }
    return false;
  },
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function getPreference(name) {
  return Services.prefs.getBoolPref(`devtools.recordreplay.${name}`);
}

const loggingFullEnabled = getPreference("loggingFull");
const loggingEnabled = loggingFullEnabled || getPreference("logging");

// eslint-disable-next-line no-unused-vars
function ConnectDebugger(dbg) {
  gDebugger = dbg;
  dbg._control = gControl;
}

const startTime = Date.now();

// eslint-disable-next-line no-unused-vars
function currentTime() {
  return (((Date.now() - startTime) / 10) | 0) / 100;
}

function dumpv(str) {
  if (loggingEnabled) {
    dump(`[ReplayControl ${currentTime()}] ${str}\n`);
  }
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function ThrowError(msg) {
  const error = new Error(msg);
  dump(`ReplayControl Server Error: ${msg} Stack: ${error.stack}\n`);
  throw error;
}

function stringify(object) {
  const str = JSON.stringify(object);
  if (str && str.length >= 4096 && !loggingFullEnabled) {
    return `${str.substr(0, 4096)} TRIMMED ${str.length}`;
  }
  return str;
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "Initialize",
  "ConnectDebugger",
  "ManifestFinished",
  "BeforeSaveRecording",
  "AfterSaveRecording",
  "ChildCrashed",
  "PingResponse",
];
