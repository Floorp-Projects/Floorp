/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions, snapshotState: states } = require("../constants");
const { immutableUpdate } = require("../utils");

const handlers = Object.create(null);

handlers[actions.SNAPSHOT_ERROR] = function (snapshots, { id, error }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.ERROR, error })
      : snapshot;
  });
};

handlers[actions.TAKE_SNAPSHOT_START] = function (snapshots, { snapshot }) {
  return [...snapshots, snapshot];
};

handlers[actions.TAKE_SNAPSHOT_END] = function (snapshots, { id, path }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.SAVED, path })
      : snapshot;
  });
};

handlers[actions.IMPORT_SNAPSHOT_START] = handlers[actions.TAKE_SNAPSHOT_START];

handlers[actions.READ_SNAPSHOT_START] = function (snapshots, { id }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.READING })
      : snapshot;
  });
};

handlers[actions.READ_SNAPSHOT_END] = function (snapshots, { id, creationTime }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.READ, creationTime })
      : snapshot;
  });
};

handlers[actions.TAKE_CENSUS_START] = function (snapshots, { id, breakdown, inverted, filter }) {
  const census = {
    report: null,
    breakdown,
    inverted,
    filter,
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.SAVING_CENSUS, census })
      : snapshot;
  });
};

handlers[actions.TAKE_CENSUS_END] = function (snapshots, { id, report, breakdown, inverted, filter }) {
  const census = {
    report,
    breakdown,
    inverted,
    filter,
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.SAVED_CENSUS, census })
      : snapshot;
  });
};

handlers[actions.SELECT_SNAPSHOT] = function (snapshots, { id }) {
  return snapshots.map(s => immutableUpdate(s, { selected: s.id === id }));
};

handlers[actions.TOGGLE_DIFFING] = function (snapshots, action) {
  return snapshots.map(s => immutableUpdate(s, { selected: false }));
};

module.exports = function (snapshots = [], action) {
  const handler = handlers[action.type];
  if (handler) {
    return handler(snapshots, action);
  }
  return snapshots;
};
