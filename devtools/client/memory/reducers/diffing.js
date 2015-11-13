/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { actions, diffingState } = require("../constants");
const { immutableUpdate, snapshotIsDiffable } = require("../utils");

const handlers = Object.create(null);

handlers[actions.TOGGLE_DIFFING] = function (diffing, action) {
  if (diffing) {
    return null;
  }

  return Object.freeze({
    firstSnapshotId: null,
    secondSnapshotId: null,
    census: null,
    state: diffingState.SELECTING,
  });
};

handlers[actions.SELECT_SNAPSHOT_FOR_DIFFING] = function (diffing, { snapshot }) {
  assert(diffing,
         "Should never select a snapshot for diffing when we aren't diffing " +
         "anything");
  assert(diffing.state === diffingState.SELECTING,
         "Can't select when not in SELECTING state");
  assert(snapshotIsDiffable(snapshot),
         "snapshot must be in a diffable state");

  if (!diffing.firstSnapshotId) {
    return immutableUpdate(diffing, {
      firstSnapshotId: snapshot.id
    });
  }

  assert(!diffing.secondSnapshotId,
         "If we aren't selecting the first, then we must be selecting the " +
         "second");

  if (snapshot.id === diffing.firstSnapshotId) {
    // Ignore requests to select the same snapshot.
    return diffing;
  }

  return immutableUpdate(diffing, {
    secondSnapshotId: snapshot.id
  });
};

handlers[actions.TAKE_CENSUS_DIFF_START] = function (diffing, action) {
  assert(diffing, "Should be diffing when starting a census diff");
  assert(action.first.id === diffing.firstSnapshotId,
         "First snapshot's id should match");
  assert(action.second.id === diffing.secondSnapshotId,
         "Second snapshot's id should match");

  return immutableUpdate(diffing, {
    state: diffingState.TAKING_DIFF,
    census: {
      report: null,
      inverted: action.inverted,
      filter: action.filter,
      breakdown: action.breakdown,
    }
  });
};

handlers[actions.TAKE_CENSUS_DIFF_END] = function (diffing, action) {
  assert(diffing, "Should be diffing when ending a census diff");
  assert(action.first.id === diffing.firstSnapshotId,
         "First snapshot's id should match");
  assert(action.second.id === diffing.secondSnapshotId,
         "Second snapshot's id should match");

  return immutableUpdate(diffing, {
    state: diffingState.TOOK_DIFF,
    census: {
      report: action.report,
      inverted: action.inverted,
      filter: action.filter,
      breakdown: action.breakdown,
    }
  });
};

handlers[actions.DIFFFING_ERROR] = function (diffing, action) {
  return {
    state: diffingState.ERROR,
    error: action.error
  };
};

module.exports = function (diffing = null, action) {
  const handler = handlers[action.type];
  return handler ? handler(diffing, action) : diffing;
};
