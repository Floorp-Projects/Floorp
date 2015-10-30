/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actions = exports.actions = {};

// Fired by UI to request a snapshot from the actor.
actions.TAKE_SNAPSHOT_START = "take-snapshot-start";
actions.TAKE_SNAPSHOT_END = "take-snapshot-end";

// When a heap snapshot is read into memory -- only fired
// once per snapshot.
actions.READ_SNAPSHOT_START = "read-snapshot-start";
actions.READ_SNAPSHOT_END = "read-snapshot-end";

// When a census is being performed on a heap snapshot
actions.TAKE_CENSUS_START = "take-census-start";
actions.TAKE_CENSUS_END = "take-census-end";

// When requesting that the server start/stop recording allocation stacks.
actions.TOGGLE_RECORD_ALLOCATION_STACKS_START = "toggle-record-allocation-stacks-start";
actions.TOGGLE_RECORD_ALLOCATION_STACKS_END = "toggle-record-allocation-stacks-end";

// Fired by UI to select a snapshot to view.
actions.SELECT_SNAPSHOT = "select-snapshot";

// Fired to toggle tree inversion on or off.
actions.TOGGLE_INVERTED = "toggle-inverted";

// Fired to set a new breakdown.
actions.SET_BREAKDOWN = "set-breakdown";

// Fired when there is an error processing a snapshot or taking a census.
actions.SNAPSHOT_ERROR = "snapshot-error";

// Options passed to MemoryFront's startRecordingAllocations never change.
exports.ALLOCATION_RECORDING_OPTIONS = {
  probability: 1,
  maxLogLength: 1
};

const COUNT = { by: "count", count: true, bytes: true };
const INTERNAL_TYPE = { by: "internalType", then: COUNT };
const ALLOCATION_STACK = { by: "allocationStack", then: COUNT, noStack: COUNT };
const OBJECT_CLASS = { by: "objectClass", then: COUNT, other: COUNT };

const breakdowns = exports.breakdowns = {
  coarseType: {
    displayName: "Coarse Type",
    breakdown: {
      by: "coarseType",
      objects: OBJECT_CLASS,
      strings: COUNT,
      scripts: INTERNAL_TYPE,
      other: INTERNAL_TYPE,
    }
  },

  allocationStack: {
    displayName: "Allocation Stack",
    breakdown: ALLOCATION_STACK,
  },

  objectClass: {
    displayName: "Object Class",
    breakdown: OBJECT_CLASS,
  },

  internalType: {
    displayName: "Internal Type",
    breakdown: INTERNAL_TYPE,
  },
};

const snapshotState = exports.snapshotState = {};

/**
 * Various states a snapshot can be in.
 * An FSM describing snapshot states:
 *
 *     SAVING -> SAVED -> READING -> READ   <-  <-  <- SAVED_CENSUS
 *                                        ↘             ↗
 *                                         SAVING_CENSUS
 *
 * Any of these states may go to the ERROR state, from which they can never
 * leave (mwah ha ha ha!)
 */
snapshotState.ERROR = "snapshot-state-error";
snapshotState.SAVING = "snapshot-state-saving";
snapshotState.SAVED = "snapshot-state-saved";
snapshotState.READING = "snapshot-state-reading";
snapshotState.READ = "snapshot-state-read";
snapshotState.SAVING_CENSUS = "snapshot-state-saving-census";
snapshotState.SAVED_CENSUS = "snapshot-state-saved-census";
