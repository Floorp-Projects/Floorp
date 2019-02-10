/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy */

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
  "addDebuggerToGlobal(this);",
  sandbox
);
const RecordReplayControl = sandbox.RecordReplayControl;
const Services = sandbox.Services;

const InvalidCheckpointId = 0;
const FirstCheckpointId = 1;

const gChildren = [];

let gDebugger;

function ChildProcess(id, recording, role) {
  assert(!gChildren[id]);
  gChildren[id] = this;

  this.id = id;
  this.recording = recording;
  this.role = role;
  this.paused = false;

  this.lastPausePoint = null;
  this.lastPauseAtRecordingEndpoint = false;

  // The pauseNeeded flag indicates that background replaying children should
  // not resume execution once the process has paused.
  this.pauseNeeded = false;

  // All currently installed breakpoints
  this.breakpoints = [];

  // Any debugger requests sent while paused at the current point.
  this.debuggerRequests = [];

  this._willSaveCheckpoints = [];
  this._majorCheckpoints = [];
  this._minorCheckpoints = new Set();

  // Replaying processes always save the first checkpoint.
  if (!recording) {
    this._willSaveCheckpoints.push(FirstCheckpointId);
  }

  dumpv(`InitRole #${this.id} ${role.name}`);
  this.role.initialize(this, { startup: true });
}

ChildProcess.prototype = {
  hitExecutionPoint(msg) {
    assert(!this.paused);
    this.paused = true;
    this.lastPausePoint = msg.point;
    this.lastPauseAtRecordingEndpoint = msg.recordingEndpoint;

    this.role.hitExecutionPoint(msg);
  },

  setRole(role) {
    dumpv(`SetRole #${this.id} ${role.name}`);

    this.role = role;
    this.role.initialize(this, { startup: false });
  },

  addMajorCheckpoint(checkpointId) {
    this._majorCheckpoints.push(checkpointId);
  },

  addMinorCheckpoint(checkpointId) {
    this._minorCheckpoints.add(checkpointId);
  },

  _unpause() {
    this.paused = false;
    this.debuggerRequests.length = 0;
  },

  sendResume({ forward }) {
    assert(this.paused);
    this._unpause();
    RecordReplayControl.sendResume(this.id, forward);
  },

  sendRestoreCheckpoint(checkpoint) {
    assert(this.paused);
    this._unpause();
    RecordReplayControl.sendRestoreCheckpoint(this.id, checkpoint);
  },

  sendRunToPoint(point) {
    assert(this.paused);
    this._unpause();
    RecordReplayControl.sendRunToPoint(this.id, point);
  },

  sendFlushRecording() {
    assert(this.paused);
    RecordReplayControl.sendFlushRecording(this.id);
  },

  waitUntilPaused(maybeCreateCheckpoint) {
    if (this.paused) {
      return;
    }
    const msg =
      RecordReplayControl.waitUntilPaused(this.id, maybeCreateCheckpoint);
    this.hitExecutionPoint(msg);
    assert(this.paused);
  },

  lastCheckpoint() {
    return this.lastPausePoint.checkpoint;
  },

  rewindTargetCheckpoint() {
    return this.lastPausePoint.position
           ? this.lastCheckpoint()
           : this.lastCheckpoint() - 1;
  },

  // Get the last major checkpoint at or before id.
  lastMajorCheckpointPreceding(id) {
    let last = InvalidCheckpointId;
    for (const major of this._majorCheckpoints) {
      if (major > id) {
        break;
      }
      last = major;
    }
    return last;
  },

  isMajorCheckpoint(id) {
    return this._majorCheckpoints.some(major => major == id);
  },

  isMinorCheckpoint(id) {
    return this._minorCheckpoints.has(id);
  },

  ensureCheckpointSaved(id, shouldSave) {
    const willSaveIndex = this._willSaveCheckpoints.indexOf(id);
    if (shouldSave != (willSaveIndex != -1)) {
      if (shouldSave) {
        this._willSaveCheckpoints.push(id);
      } else {
        const last = this._willSaveCheckpoints.pop();
        if (willSaveIndex != this._willSaveCheckpoints.length) {
          this._willSaveCheckpoints[willSaveIndex] = last;
        }
      }
      RecordReplayControl.sendSetSaveCheckpoint(this.id, id, shouldSave);
    }
  },

  // Ensure a checkpoint is saved in this child iff it is a major one.
  ensureMajorCheckpointSaved(id) {
    // The first checkpoint is always saved, even if not marked as major.
    this.ensureCheckpointSaved(id, this.isMajorCheckpoint(id) || id == FirstCheckpointId);
  },

  hasSavedCheckpoint(id) {
    return (id <= this.lastCheckpoint()) &&
           this._willSaveCheckpoints.includes(id);
  },

  // Return whether this child has saved all minor checkpoints between the last
  // major checkpoint preceding to id and id itself. This is required in order
  // for the child to rewind through this span of checkpoints.
  canRewindFrom(id) {
    const lastMajorCheckpoint = this.lastMajorCheckpointPreceding(id);
    for (let i = lastMajorCheckpoint + 1; i <= id; i++) {
      if (this.isMinorCheckpoint(i) && !this.hasSavedCheckpoint(i)) {
        return false;
      }
    }
    return true;
  },

  lastSavedCheckpointPriorTo(id) {
    while (!this.hasSavedCheckpoint(id)) {
      id--;
    }
    return id;
  },

  sendAddBreakpoint(pos) {
    assert(this.paused);
    this.breakpoints.push(pos);
    RecordReplayControl.sendAddBreakpoint(this.id, pos);
  },

  sendClearBreakpoints() {
    assert(this.paused);
    this.breakpoints.length = 0;
    RecordReplayControl.sendClearBreakpoints(this.id);
  },

  sendDebuggerRequest(request) {
    assert(this.paused);
    this.debuggerRequests.push(request);
    return RecordReplayControl.sendDebuggerRequest(this.id, request);
  },

  // When a background child pauses, it does not immediately resume. This will
  // asynchronously let the role know that it may be able to make progress,
  // depending on where the active child is and what it is doing.
  pokeSoon() {
    if (!this.recording) {
      Services.tm.dispatchToMainThread(() => {
        if (this.paused) {
          this.role.poke();
        }
      });
    }
  },
};

function pokeChildren() {
  for (const child of gChildren) {
    if (child) {
      child.pokeSoon();
    }
  }
}

const FlushMs = .5 * 1000;
const MajorCheckpointMs = 2 * 1000;
const MinorCheckpointMs = .25 * 1000;

// This section describes the strategy used for managing child processes. When
// recording, there is a single recording process and two replaying processes.
// When replaying, there are two replaying processes. The main advantage of
// using two replaying processes is to provide a smooth experience when
// rewinding.
//
// At any time there is one active child: the process which the user is
// interacting with. This may be any of the two or three children in existence,
// depending on the user's behavior. The other processes do not interact with
// the user: inactive recording processes are inert, and sit idle until
// recording is ready to resume, while inactive replaying processes are on
// standby, staying close to the active process in the recording's execution
// space and saving checkpoints in case the user starts rewinding.
//
// Below are some scenarios showing the state we attempt to keep the children
// in, and ways in which the active process switches from one to another.
// The execution diagrams show the position of each process, with '*' and '-'
// indicating checkpoints the process reached and, respectively, whether
// the checkpoint was saved or not.
//
// When the recording process is actively recording, flushes are issued to it
// every FlushMs to keep the recording reasonably current and allow the
// replaying processes to stay behind but close to the position of the
// recording process. Additionally, one replaying process saves a checkpoint
// every MajorCheckpointMs with the process saving the checkpoint alternating
// back and forth so that individual processes save checkpoints every
// MajorCheckpointMs*2. These are the major checkpoints for each replaying
// process.
//
// Active  Recording:    -----------------------
// Standby Replaying #1: *---------*---------*
// Standby Replaying #2: -----*---------*-----
//
// When the recording process is explicitly paused (via the debugger UI) at a
// checkpoint or breakpoint, it is flushed and the replaying processes will
// navigate around the recording to save a second set of checkpoints going back
// at least MajorCheckpointSeconds, with the goal of making sure saved
// checkpoints are no more than MinorCheckpointSeconds apart. No replaying
// process needs to rewind past its last major checkpoint, and a given
// minor checkpoint will only ever be saved by the replaying process with the
// most recent major checkpoint.
//
// Active  Recording:    -----------------------
// Standby Replaying #1: *---------*---------*-*
// Standby Replaying #2: -----*---------*-*-*
//
// If the user starts rewinding, the replaying process with the most recent
// major checkpoint (and which has been saving the most recent minor
// checkpoints) becomes the active child.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------*---------*-
// Standby Replaying #2: -----*---------*-*-*
//
// As the user continues rewinding, the replaying process stays active until it
// goes past its most recent major checkpoint. At that time the other replaying
// process (which has been saving checkpoints prior to that point) becomes the
// active child and allows continuous rewinding. The first replaying process
// rewinds to its last major checkpoint and begins saving older minor
// checkpoints, attempting to maintain the invariant that we have saved (or are
// saving) all checkpoints going back MajorCheckpointMs.
//
// Inert   Recording:    -----------------------
// Standby Replaying #1: *---------*-*-*
// Active  Replaying #2: -----*---------*-
//
// Rewinding continues in this manner, alternating back and forth between the
// replaying processes as the user continues going back in time.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------*-*
// Standby Replaying #2: -----*-*-*
//
// If the user starts navigating forward, the replaying processes both run
// forward and save checkpoints at the same major checkpoints as earlier.
// Note that this is how all forward execution works when there is no recording
// process (i.e. we started from a saved recording).
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------*-*-----
// Standby Replaying #2: -----*-*-*-----*--
//
// If the user pauses at a checkpoint or breakpoint in the replay, we again
// want to fill in all the checkpoints going back MajorCheckpointMs to allow
// smooth rewinding. This cannot be done simultaneously -- as it was when the
// recording process was active -- since we need to keep one of the replaying
// processes at an up to date point and be the active one. This falls on the one
// whose most recent major checkpoint is oldest, as the other is responsible for
// saving the most recent minor checkpoints.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------*-*-----
// Standby Replaying #2: -----*-*-*-----*-*
//
// After the recent minor checkpoints have been saved the process which
// took them can become active so the older minor checkpoints can be
// saved.
//
// Inert   Recording:    -----------------------
// Standby Replaying #1: *---------*-*-*
// Active  Replaying #2: -----*-*-*-----*-*
//
// Finally, if the replay plays forward to the end of the recording (the point
// where the recording process is situated), the recording process takes over
// again as the active child and the user can resume interacting with a live
// process.
//
// Active  Recording:    ----------------------------------------
// Standby Replaying #1: *---------*-*-*-----*---------*-------
// Standby Replaying #2: -----*-*-*-----*-*-------*---------*--

// Child processes that can participate in the above management.
let gRecordingChild;
let gFirstReplayingChild;
let gSecondReplayingChild;
let gActiveChild;

function otherReplayingChild(child) {
  assert(child == gFirstReplayingChild || child == gSecondReplayingChild);
  return child == gFirstReplayingChild
         ? gSecondReplayingChild
         : gFirstReplayingChild;
}

////////////////////////////////////////////////////////////////////////////////
// Child Roles
////////////////////////////////////////////////////////////////////////////////

function ChildRoleActive() {}

ChildRoleActive.prototype = {
  name: "Active",

  initialize(child, { startup }) {
    this.child = child;
    gActiveChild = child;

    // Mark the child as active unless we are starting up, in which case it is
    // unpaused and we can't send messages to it.
    if (!startup) {
      RecordReplayControl.setActiveChild(child.id);
    }
  },

  hitExecutionPoint(msg) {
    // Ignore HitCheckpoint messages received while doing a time warp.
    // timeWarp() will immediately resume the child and we don't want to tell
    // the debugger it ever paused.
    if (gTimeWarpInProgress) {
      return;
    }

    // Make sure the active child is marked as such when starting up.
    if (msg.point.checkpoint == FirstCheckpointId) {
      RecordReplayControl.setActiveChild(this.child.id);
    }

    updateCheckpointTimes(msg);

    // When at the endpoint of the recording, immediately resume. We don't
    // want to notify the debugger about this: if the user installed a
    // breakpoint here we will have already gotten a HitExecutionPoint message
    // *without* mRecordingEndpoint set, and we don't want to pause twice at
    // the same point.
    if (msg.recordingEndpoint) {
      resume(true);

      // When resuming at the end of the recording, we will either switch to a
      // recording child or stay paused at the endpoint. In either case, this
      // process will stay paused.
      assert(this.child.paused);
      return;
    }

    // Run forward by default if there is no debugger attached, but post a
    // runnable so that callers waiting for the child to pause don't starve.
    if (!gDebugger) {
      Services.tm.dispatchToMainThread(() => this.child.sendResume({ forward: true }));
      return;
    }

    gDebugger._onPause();
  },

  poke() {},
};

// The last checkpoint included in the recording.
let gLastRecordingCheckpoint;

// The role taken by replaying children trying to stay close to the active
// child and save either major or minor checkpoints, depending on whether the
// active child is paused or rewinding.
function ChildRoleStandby() {}

ChildRoleStandby.prototype = {
  name: "Standby",

  initialize(child) {
    this.child = child;
    this.child.pokeSoon();
  },

  hitExecutionPoint(msg) {
    assert(!msg.point.position);
    this.child.pokeSoon();
  },

  poke() {
    assert(this.child.paused && !this.child.lastPausePoint.position);
    const currentCheckpoint = this.child.lastCheckpoint();

    // Stay paused if we need to while the recording is flushed.
    if (this.child.pauseNeeded) {
      return;
    }

    // Minor checkpoints are only saved when the active child is paused
    // or rewinding.
    let targetCheckpoint = getActiveChildTargetCheckpoint();
    if (targetCheckpoint == undefined) {
      // Minor checkpoints do not need to be saved. Run forward until we
      // reach either the active child's position, or the last checkpoint
      // included in the on-disk recording. Only save major checkpoints.
      if ((currentCheckpoint < gActiveChild.lastCheckpoint()) &&
          (!gRecordingChild || currentCheckpoint < gLastRecordingCheckpoint)) {
        this.child.ensureMajorCheckpointSaved(currentCheckpoint + 1);
        this.child.sendResume({ forward: true });
      }
      return;
    }

    // The startpoint of the range is the most recent major checkpoint prior
    // to the target.
    const lastMajorCheckpoint =
      this.child.lastMajorCheckpointPreceding(targetCheckpoint);

    // If there is no major checkpoint prior to the target, just idle.
    if (lastMajorCheckpoint == InvalidCheckpointId) {
      return;
    }

    // If we haven't reached the last major checkpoint, we need to run forward
    // without saving minor checkpoints.
    if (currentCheckpoint < lastMajorCheckpoint) {
      this.child.ensureMajorCheckpointSaved(currentCheckpoint + 1);
      this.child.sendResume({ forward: true });
      return;
    }

    // The endpoint of the range is the checkpoint prior to either the active
    // child's current position, or the other replaying child's most recent
    // major checkpoint.
    const otherChild = otherReplayingChild(this.child);
    const otherMajorCheckpoint =
      otherChild.lastMajorCheckpointPreceding(targetCheckpoint);
    if (otherMajorCheckpoint > lastMajorCheckpoint) {
      assert(otherMajorCheckpoint <= targetCheckpoint);
      targetCheckpoint = otherMajorCheckpoint - 1;
    }

    // Find the first minor checkpoint in the fill range which we have not saved.
    let missingCheckpoint;
    for (let i = lastMajorCheckpoint + 1; i <= targetCheckpoint; i++) {
      if (this.child.isMinorCheckpoint(i) && !this.child.hasSavedCheckpoint(i)) {
        missingCheckpoint = i;
        break;
      }
    }

    // If we have already saved everything we need to, we can idle.
    if (missingCheckpoint == undefined) {
      return;
    }

    if (this.child.lastCheckpoint() < missingCheckpoint) {
      // We can run forward to reach the missing checkpoint.
    } else {
      // We need to rewind in order to save the missing checkpoint. Find the
      // last saved checkpoint prior to the missing one. This must be
      // lastMajorCheckpoint or later, as we always save major checkpoints.
      let restoreTarget = missingCheckpoint - 1;
      while (!this.child.hasSavedCheckpoint(restoreTarget)) {
        restoreTarget--;
      }
      assert(restoreTarget >= lastMajorCheckpoint);

      this.child.sendRestoreCheckpoint(restoreTarget);
      return;
    }

    // Make sure the process will save minor checkpoints as it runs forward.
    if (missingCheckpoint == this.child.lastCheckpoint() + 1) {
      this.child.ensureCheckpointSaved(missingCheckpoint, true);
    }

    // Run forward to the next checkpoint.
    this.child.sendResume({ forward: true });
  },
};

// The role taken by a child that always sits idle.
function ChildRoleInert() {}

ChildRoleInert.prototype = {
  name: "Inert",

  initialize() {},
  hitExecutionPoint() {},
  poke() {},
};

////////////////////////////////////////////////////////////////////////////////
// Child Switching
////////////////////////////////////////////////////////////////////////////////

// Change the current active child, and select a new role for the old one.
function switchActiveChild(child, recoverPosition = true) {
  assert(child != gActiveChild);
  assert(gActiveChild.paused);

  const oldActiveChild = gActiveChild;
  child.waitUntilPaused();

  // Move the installed breakpoints from the old child to the new child.
  assert(child.breakpoints.length == 0);
  for (const pos of oldActiveChild.breakpoints) {
    child.sendAddBreakpoint(pos);
  }
  oldActiveChild.sendClearBreakpoints();

  if (recoverPosition && !child.recording) {
    child.setRole(new ChildRoleInert());
    const targetCheckpoint = oldActiveChild.lastCheckpoint();
    if (child.lastCheckpoint() > targetCheckpoint) {
      const restoreCheckpoint =
        child.lastSavedCheckpointPriorTo(targetCheckpoint);
      child.sendRestoreCheckpoint(restoreCheckpoint);
      child.waitUntilPaused();
    }
    while (child.lastCheckpoint() < targetCheckpoint) {
      child.ensureMajorCheckpointSaved(child.lastCheckpoint() + 1);
      child.sendResume({ forward: true });
      child.waitUntilPaused();
    }
    assert(!child.lastPausePoint.position);
    if (oldActiveChild.lastPausePoint.position) {
      child.sendRunToPoint(oldActiveChild.lastPausePoint);
      child.waitUntilPaused();
    }
    for (const request of oldActiveChild.debuggerRequests) {
      child.sendDebuggerRequest(request);
    }
  }

  child.setRole(new ChildRoleActive());
  oldActiveChild.setRole(new ChildRoleInert());

  if (!oldActiveChild.recording) {
    if (oldActiveChild.lastPausePoint.position) {
      // Standby replaying children must be paused at a checkpoint.
      const oldCheckpoint = oldActiveChild.lastCheckpoint();
      const restoreCheckpoint =
        oldActiveChild.lastSavedCheckpointPriorTo(oldCheckpoint);
      oldActiveChild.sendRestoreCheckpoint(restoreCheckpoint);
      oldActiveChild.waitUntilPaused();
    }
    oldActiveChild.setRole(new ChildRoleStandby());
  }

  // Notify the debugger when switching between recording and replaying
  // children.
  if (child.recording != oldActiveChild.recording) {
    gDebugger._onSwitchChild();
  }
}

function maybeSwitchToReplayingChild() {
  if (gActiveChild.recording && RecordReplayControl.canRewind()) {
    flushRecording();
    const checkpoint = gActiveChild.rewindTargetCheckpoint();
    const child = otherReplayingChild(
      replayingChildResponsibleForSavingCheckpoint(checkpoint));
    switchActiveChild(child);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Major and Minor Checkpoints
////////////////////////////////////////////////////////////////////////////////

// For each checkpoint N, this vector keeps track of the time intervals taken
// for the active child (excluding idle time) to run from N to N+1.
const gCheckpointTimes = [];

// How much time has elapsed (per gCheckpointTimes) since the last flush or
// major/minor checkpoint was noted.
let gTimeSinceLastFlush;
let gTimeSinceLastMajorCheckpoint = 0;
let gTimeSinceLastMinorCheckpoint = 0;

// The replaying process that was given the last major checkpoint.
let gLastAssignedMajorCheckpoint;

function assignMajorCheckpoint(child, checkpointId) {
  dumpv(`AssignMajorCheckpoint: #${child.id} Checkpoint ${checkpointId}`);
  child.addMajorCheckpoint(checkpointId);
  gLastAssignedMajorCheckpoint = child;
}

function assignMinorCheckpoint(child, checkpointId) {
  dumpv(`AssignMinorCheckpoint: #${child.id} Checkpoint ${checkpointId}`);
  child.addMinorCheckpoint(checkpointId);
}

function updateCheckpointTimes(msg) {
  if (msg.point.checkpoint != gCheckpointTimes.length + 1 ||
      msg.point.position) {
    return;
  }
  gCheckpointTimes.push(msg.duration);

  if (gActiveChild.recording) {
    gTimeSinceLastFlush += msg.duration;

    // Occasionally flush while recording so replaying processes stay
    // reasonably current.
    if (msg.point.checkpoint == FirstCheckpointId ||
        gTimeSinceLastFlush >= FlushMs) {
      if (maybeFlushRecording()) {
        gTimeSinceLastFlush = 0;
      }
    }
  }

  gTimeSinceLastMajorCheckpoint += msg.duration;
  gTimeSinceLastMinorCheckpoint += msg.duration;

  if (gTimeSinceLastMajorCheckpoint >= MajorCheckpointMs) {
    // Alternate back and forth between assigning major checkpoints to the
    // two replaying processes.
    const child = otherReplayingChild(gLastAssignedMajorCheckpoint);
    assignMajorCheckpoint(child, msg.point.checkpoint + 1);
    gTimeSinceLastMajorCheckpoint = 0;
  } else if (gTimeSinceLastMinorCheckpoint >= MinorCheckpointMs) {
    // Assign a minor checkpoint to the process which saved the last major one.
    assignMinorCheckpoint(gLastAssignedMajorCheckpoint, msg.point.checkpoint + 1);
    gTimeSinceLastMinorCheckpoint = 0;
  }
}

// Get the replaying process responsible for saving id when rewinding: the one
// with the most recent major checkpoint preceding id.
function replayingChildResponsibleForSavingCheckpoint(id) {
  assert(gFirstReplayingChild && gSecondReplayingChild);
  const firstMajor = gFirstReplayingChild.lastMajorCheckpointPreceding(id);
  const secondMajor = gSecondReplayingChild.lastMajorCheckpointPreceding(id);
  return (firstMajor < secondMajor)
         ? gSecondReplayingChild
         : gFirstReplayingChild;
}

////////////////////////////////////////////////////////////////////////////////
// Saving Recordings
////////////////////////////////////////////////////////////////////////////////

// Synchronously flush the recording to disk.
function flushRecording() {
  assert(gActiveChild.recording && gActiveChild.paused);

  // All replaying children must be paused while the recording is flushed.
  for (const child of gChildren) {
    if (child && !child.recording) {
      child.waitUntilPaused();
    }
  }

  gActiveChild.sendFlushRecording();

  // Clear out pauseNeeded state set by any earlier maybeFlushRecording().
  for (const child of gChildren) {
    if (child && !child.recording) {
      child.pauseNeeded = false;
      child.pokeSoon();
    }
  }

  // After flushing the recording there may be more search results.
  maybeResumeSearch();

  gLastRecordingCheckpoint = gActiveChild.lastCheckpoint();

  // We now have a usable recording for replaying children.
  if (!gFirstReplayingChild) {
    spawnInitialReplayingChildren();
  }
}

// Get the replaying children to pause, and flush the recording if they already
// are.
function maybeFlushRecording() {
  assert(gActiveChild.recording && gActiveChild.paused);

  let allPaused = true;
  for (const child of gChildren) {
    if (child && !child.recording) {
      child.pauseNeeded = true;
      allPaused &= child.paused;
    }
  }

  if (allPaused) {
    flushRecording();
    return true;
  }
  return false;
}

// eslint-disable-next-line no-unused-vars
function BeforeSaveRecording() {
  if (gActiveChild.recording) {
    // The recording might not be up to date, flush it now.
    gActiveChild.waitUntilPaused(true);
    flushRecording();
  }
}

// eslint-disable-next-line no-unused-vars
function AfterSaveRecording() {
  Services.cpmm.sendAsyncMessage("SaveRecordingFinished");
}

////////////////////////////////////////////////////////////////////////////////
// Child Management
////////////////////////////////////////////////////////////////////////////////

function spawnReplayingChild(role) {
  const id = RecordReplayControl.spawnReplayingChild();
  return new ChildProcess(id, false, role);
}

function spawnInitialReplayingChildren() {
  gFirstReplayingChild = spawnReplayingChild(gRecordingChild
                                             ? new ChildRoleStandby()
                                             : new ChildRoleActive());
  gSecondReplayingChild = spawnReplayingChild(new ChildRoleStandby());

  assignMajorCheckpoint(gSecondReplayingChild, FirstCheckpointId);
}

// eslint-disable-next-line no-unused-vars
function Initialize(recordingChildId) {
  try {
    if (recordingChildId != undefined) {
      gRecordingChild = new ChildProcess(recordingChildId, true,
                                         new ChildRoleActive());
    } else {
      // If there is no recording child, we have now initialized enough state
      // that we can start spawning replaying children.
      spawnInitialReplayingChildren();
    }
    return gControl;
  } catch (e) {
    dump(`ERROR: Initialize threw exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function HitExecutionPoint(id, msg) {
  try {
    dumpv(`HitExecutionPoint #${id} ${JSON.stringify(msg)}`);
    gChildren[id].hitExecutionPoint(msg);
  } catch (e) {
    dump(`ERROR: HitExecutionPoint threw exception: ${e}\n`);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Explicit Pauses
///////////////////////////////////////////////////////////////////////////////

// At the last time the active child was explicitly paused, the ID of the
// checkpoint that needs to be saved for the child to rewind.
let gLastExplicitPause = FirstCheckpointId;

// Returns a checkpoint if the active child is explicitly paused somewhere,
// has started rewinding after being explicitly paused, or is attempting to
// warp to an execution point. Standby roles will try to save minor checkpoints
// in the range from their most recent major checkpoint up to the returned
// checkpoint.
function getActiveChildTargetCheckpoint() {
  if (gActiveChild.rewindTargetCheckpoint() <= gLastExplicitPause) {
    return gActiveChild.rewindTargetCheckpoint();
  }
  return undefined;
}

function markExplicitPause() {
  assert(gActiveChild.paused);
  const targetCheckpoint = gActiveChild.rewindTargetCheckpoint();

  if (gActiveChild.recording) {
    // Make sure any replaying children can play forward to the same point as
    // the recording.
    flushRecording();
  } else if (RecordReplayControl.canRewind()) {
    // Make sure we have a replaying child that has saved the right checkpoints
    // for rewinding from this point. Switch to the other one if (a) this process
    // is responsible for rewinding from this point, and (b) this process has
    // not saved all minor checkpoints going back to its last major checkpoint.
    if (gActiveChild ==
        replayingChildResponsibleForSavingCheckpoint(targetCheckpoint)) {
      if (!gActiveChild.canRewindFrom(targetCheckpoint)) {
        switchActiveChild(otherReplayingChild(gActiveChild));
      }
    }
  }

  gLastExplicitPause = targetCheckpoint;
  dumpv(`MarkActiveChildExplicitPause ${gLastExplicitPause}`);

  pokeChildren();
}

////////////////////////////////////////////////////////////////////////////////
// Debugger Operations
////////////////////////////////////////////////////////////////////////////////

function maybeSendRepaintMessage() {
  // In repaint stress mode, we want to trigger a repaint at every checkpoint,
  // so before resuming after the child pauses at each checkpoint, send it a
  // repaint message. There might not be a debugger open, so manually craft the
  // same message which the debugger would send to trigger a repaint and parse
  // the result.
  if (RecordReplayControl.inRepaintStressMode()) {
    maybeSwitchToReplayingChild();
    const rv = gActiveChild.sendRequest({ type: "repaint" });
    if ("width" in rv && "height" in rv) {
      RecordReplayControl.hadRepaint(rv.width, rv.height);
    }
  }
}

function waitUntilChildHasSavedCheckpoint(child, checkpoint) {
  while (true) {
    child.waitUntilPaused();
    if (child.hasSavedCheckpoint(checkpoint)) {
      return;
    }
    child.role.poke();
  }
}

function resume(forward) {
  assert(gActiveChild.paused);

  maybeSendRepaintMessage();

  if (!forward) {
    const targetCheckpoint = gActiveChild.rewindTargetCheckpoint();

    // Don't rewind if we are at the beginning of the recording.
    if (targetCheckpoint == InvalidCheckpointId) {
      Services.cpmm.sendAsyncMessage("HitRecordingBeginning");
      gDebugger._onPause(gActiveChild.lastPausePoint);
      return;
    }

    // Make sure the active child has saved minor checkpoints prior to its
    // position.
    const targetChild =
      replayingChildResponsibleForSavingCheckpoint(targetCheckpoint);
    if (targetChild == gActiveChild) {
      // markExplicitPause() should ensure that we are only active if the child
      // has saved the appropriate minor checkpoints.
      assert(gActiveChild.canRewindFrom(targetCheckpoint));
    } else {
      let saveTarget = targetCheckpoint;
      while (!targetChild.isMajorCheckpoint(saveTarget) &&
             !targetChild.isMinorCheckpoint(saveTarget)) {
        saveTarget--;
      }
      waitUntilChildHasSavedCheckpoint(targetChild, saveTarget);
      switchActiveChild(targetChild);
    }
  }

  if (forward) {
    // Don't send a replaying process past the recording endpoint.
    if (gActiveChild.lastPauseAtRecordingEndpoint) {
      // Look for a recording child we can transition into.
      assert(!gActiveChild.recording);
      if (!gRecordingChild) {
        Services.cpmm.sendAsyncMessage("HitRecordingEndpoint");
        if (gDebugger) {
          gDebugger._onPause(gActiveChild.lastPausePoint);
        }
        return;
      }

      // Switch to the recording child as the active child and continue
      // execution.
      switchActiveChild(gRecordingChild);
    }

    gActiveChild.ensureMajorCheckpointSaved(gActiveChild.lastCheckpoint() + 1);

    // Idle children might change their behavior as we run forward.
    pokeChildren();
  }

  gActiveChild.sendResume({ forward });
}

let gTimeWarpInProgress;

function timeWarp(targetPoint) {
  assert(gActiveChild.paused);
  const targetCheckpoint = targetPoint.checkpoint;

  // Find the replaying child responsible for saving the target checkpoint.
  const targetChild =
    replayingChildResponsibleForSavingCheckpoint(targetCheckpoint);
  if (targetChild != gActiveChild) {
    switchActiveChild(otherReplayingChild(gActiveChild));
  }

  // Rewind first if the child is past the warp target or if it is not paused
  // at a checkpoint. RunToPoint can only be used when the child is at a
  // checkpoint.
  let restoreTarget;
  if (gActiveChild.lastCheckpoint() >= targetCheckpoint) {
    restoreTarget = targetCheckpoint;
  } else if (gActiveChild.lastPausePoint.position) {
    restoreTarget = gActiveChild.lastPausePoint.checkpoint;
  }

  if (restoreTarget) {
    while (!gActiveChild.hasSavedCheckpoint(restoreTarget)) {
      restoreTarget--;
    }

    assert(!gTimeWarpInProgress);
    gTimeWarpInProgress = true;

    gActiveChild.sendRestoreCheckpoint(restoreTarget);
    gActiveChild.waitUntilPaused();

    gTimeWarpInProgress = false;
  }

  gActiveChild.sendRunToPoint(targetPoint);
  gActiveChild.waitUntilPaused();

  Services.cpmm.sendAsyncMessage("TimeWarpFinished");
}

const gControl = {
  pausePoint() { return gActiveChild.paused ? gActiveChild.lastPausePoint : null; },
  childIsRecording() { return gActiveChild.recording; },
  waitUntilPaused() {
    // Use a loop because the active child can change while running if a
    // replaying active child hits the end of the recording.
    while (!gActiveChild.paused) {
      gActiveChild.waitUntilPaused(true);
    }
  },
  addBreakpoint(pos) { gActiveChild.sendAddBreakpoint(pos); },
  clearBreakpoints() { gActiveChild.sendClearBreakpoints(); },
  sendRequest(request) { return gActiveChild.sendDebuggerRequest(request); },
  markExplicitPause,
  maybeSwitchToReplayingChild,
  resume,
  timeWarp,
};

////////////////////////////////////////////////////////////////////////////////
// Search Operations
////////////////////////////////////////////////////////////////////////////////

let gSearchChild;

function ChildRoleSearch() {}

ChildRoleSearch.prototype = {
  name: "Search",

  initialize(child, { startup }) {
    this.child = child;
  },

  hitExecutionPoint({ point, recordingEndpoint }) {
    if (point.position) {
      gDebugger._onSearchPause(point);
    }

    if (!recordingEndpoint) {
      this.child.pokeSoon();
    }
  },

  poke() {
    if (!this.child.pauseNeeded) {
      this.child.sendResume({ forward: true });
    }
  },
};

function ensureHasSearchChild() {
  if (!gSearchChild) {
    gSearchChild = spawnReplayingChild(new ChildRoleSearch());
  }
}

function maybeResumeSearch() {
  if (gSearchChild && gSearchChild.paused) {
    gSearchChild.sendResume({ forward: true });
  }
}

const gSearchControl = {
  reset() {
    ensureHasSearchChild();
    gSearchChild.waitUntilPaused();

    if (gSearchChild.lastPausePoint.checkpoint != FirstCheckpointId ||
        gSearchChild.lastPausePoint.position) {
      gSearchChild.sendRestoreCheckpoint(FirstCheckpointId);
      gSearchChild.waitUntilPaused();
    }
    gSearchChild.sendClearBreakpoints();
    gDebugger._forEachSearch(pos => gSearchChild.sendAddBreakpoint(pos));
    gSearchChild.sendResume({ forward: true });
  },

  sendRequest(request) { return gSearchChild.sendDebuggerRequest(request); },
};

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

// eslint-disable-next-line no-unused-vars
function ConnectDebugger(dbg) {
  gDebugger = dbg;
  dbg._control = gControl;
  dbg._searchControl = gSearchControl;
}

function dumpv(str) {
  //dump("[ReplayControl] " + str + "\n");
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function ThrowError(msg)
{
  const error = new Error(msg);
  dump("ReplayControl Server Error: " + msg + " Stack: " + error.stack + "\n");
  throw error;
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "Initialize",
  "ConnectDebugger",
  "HitExecutionPoint",
  "BeforeSaveRecording",
  "AfterSaveRecording",
];
