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

// Fired by UI to select a snapshot to view.
actions.SELECT_SNAPSHOT = "select-snapshot";

const COUNT = { by: "count", count: true, bytes: true };
const INTERNAL_TYPE = { by: "internalType", then: COUNT };
const ALLOCATION_STACK = { by: "allocationStack", then: COUNT, noStack: COUNT };
const OBJECT_CLASS = { by: "objectClass", then: COUNT, other: COUNT };

const breakdowns = exports.breakdowns = {
  coarseType: {
    displayName: "Coarse Type",
    breakdown: {
      by: "coarseType",
      objects: ALLOCATION_STACK,
      strings: ALLOCATION_STACK,
      scripts: INTERNAL_TYPE,
      other: INTERNAL_TYPE,
    }
  },

  allocationStack: {
    displayName: "Allocation Site",
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
 * SAVING -> SAVED -> READING -> READ   <-  <-  <- SAVED_CENSUS
 *                                    ↘             ↗
 *                                     SAVING_CENSUS
 */
snapshotState.SAVING = "snapshot-state-saving";
snapshotState.SAVED = "snapshot-state-saved";
snapshotState.READING = "snapshot-state-reading";
snapshotState.READ = "snapshot-state-read";
snapshotState.SAVING_CENSUS = "snapshot-state-saving-census";
snapshotState.SAVED_CENSUS = "snapshot-state-saved-census";
