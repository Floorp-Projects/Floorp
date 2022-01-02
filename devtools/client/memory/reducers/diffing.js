/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const { immutableUpdate, assert } = require("devtools/shared/DevToolsUtils");
const {
  actions,
  diffingState,
  viewState,
} = require("devtools/client/memory/constants");
const { snapshotIsDiffable } = require("devtools/client/memory/utils");

const handlers = Object.create(null);

handlers[actions.POP_VIEW] = function(diffing, { previousView }) {
  if (previousView.state === viewState.DIFFING) {
    assert(previousView.diffing, "Should have previousView.diffing");
    return previousView.diffing;
  }

  return null;
};

handlers[actions.CHANGE_VIEW] = function(diffing, { newViewState }) {
  if (newViewState === viewState.DIFFING) {
    assert(!diffing, "Should not switch to diffing view when already diffing");
    return Object.freeze({
      firstSnapshotId: null,
      secondSnapshotId: null,
      census: null,
      state: diffingState.SELECTING,
    });
  }

  return null;
};

handlers[actions.SELECT_SNAPSHOT_FOR_DIFFING] = function(
  diffing,
  { snapshot }
) {
  assert(
    diffing,
    "Should never select a snapshot for diffing when we aren't diffing " +
      "anything"
  );
  assert(
    diffing.state === diffingState.SELECTING,
    "Can't select when not in SELECTING state"
  );
  assert(snapshotIsDiffable(snapshot), "snapshot must be in a diffable state");

  if (!diffing.firstSnapshotId) {
    return immutableUpdate(diffing, {
      firstSnapshotId: snapshot.id,
    });
  }

  assert(
    !diffing.secondSnapshotId,
    "If we aren't selecting the first, then we must be selecting the " +
      "second"
  );

  if (snapshot.id === diffing.firstSnapshotId) {
    // Ignore requests to select the same snapshot.
    return diffing;
  }

  return immutableUpdate(diffing, {
    secondSnapshotId: snapshot.id,
  });
};

handlers[actions.TAKE_CENSUS_DIFF_START] = function(diffing, action) {
  assert(diffing, "Should be diffing when starting a census diff");
  assert(
    action.first.id === diffing.firstSnapshotId,
    "First snapshot's id should match"
  );
  assert(
    action.second.id === diffing.secondSnapshotId,
    "Second snapshot's id should match"
  );

  return immutableUpdate(diffing, {
    state: diffingState.TAKING_DIFF,
    census: {
      report: null,
      inverted: action.inverted,
      filter: action.filter,
      display: action.display,
    },
  });
};

handlers[actions.TAKE_CENSUS_DIFF_END] = function(diffing, action) {
  assert(diffing, "Should be diffing when ending a census diff");
  assert(
    action.first.id === diffing.firstSnapshotId,
    "First snapshot's id should match"
  );
  assert(
    action.second.id === diffing.secondSnapshotId,
    "Second snapshot's id should match"
  );

  return immutableUpdate(diffing, {
    state: diffingState.TOOK_DIFF,
    census: {
      report: action.report,
      parentMap: action.parentMap,
      expanded: Immutable.Set(),
      inverted: action.inverted,
      filter: action.filter,
      display: action.display,
    },
  });
};

handlers[actions.DIFFING_ERROR] = function(diffing, action) {
  return {
    state: diffingState.ERROR,
    error: action.error,
  };
};

handlers[actions.EXPAND_DIFFING_CENSUS_NODE] = function(diffing, { node }) {
  assert(diffing, "Should be diffing if expanding diffing's census nodes");
  assert(
    diffing.state === diffingState.TOOK_DIFF,
    "Should have taken the census diff if expanding nodes"
  );
  assert(diffing.census, "Should have a census");
  assert(diffing.census.report, "Should have a census report");
  assert(diffing.census.expanded, "Should have a census's expanded set");

  const expanded = diffing.census.expanded.add(node.id);
  const census = immutableUpdate(diffing.census, { expanded });
  return immutableUpdate(diffing, { census });
};

handlers[actions.COLLAPSE_DIFFING_CENSUS_NODE] = function(diffing, { node }) {
  assert(diffing, "Should be diffing if expanding diffing's census nodes");
  assert(
    diffing.state === diffingState.TOOK_DIFF,
    "Should have taken the census diff if expanding nodes"
  );
  assert(diffing.census, "Should have a census");
  assert(diffing.census.report, "Should have a census report");
  assert(diffing.census.expanded, "Should have a census's expanded set");

  const expanded = diffing.census.expanded.delete(node.id);
  const census = immutableUpdate(diffing.census, { expanded });
  return immutableUpdate(diffing, { census });
};

handlers[actions.FOCUS_DIFFING_CENSUS_NODE] = function(diffing, { node }) {
  assert(diffing, "Should be diffing.");
  assert(diffing.census, "Should have a census");
  const census = immutableUpdate(diffing.census, { focused: node });
  return immutableUpdate(diffing, { census });
};

module.exports = function(diffing = null, action) {
  const handler = handlers[action.type];
  return handler ? handler(diffing, action) : diffing;
};
