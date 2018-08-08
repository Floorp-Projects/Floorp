"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getPendingBreakpoints = getPendingBreakpoints;
exports.getPendingBreakpointList = getPendingBreakpointList;
exports.getPendingBreakpointsForSource = getPendingBreakpointsForSource;

var _breakpoint = require("../utils/breakpoint/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Pending breakpoints reducer
 * @module reducers/pending-breakpoints
 */
function update(state = {}, action) {
  switch (action.type) {
    case "ADD_BREAKPOINT":
      {
        return addBreakpoint(state, action);
      }

    case "SYNC_BREAKPOINT":
      {
        return syncBreakpoint(state, action);
      }

    case "ENABLE_BREAKPOINT":
      {
        return addBreakpoint(state, action);
      }

    case "DISABLE_BREAKPOINT":
      {
        return updateBreakpoint(state, action);
      }

    case "DISABLE_ALL_BREAKPOINTS":
      {
        return updateAllBreakpoints(state, action);
      }

    case "ENABLE_ALL_BREAKPOINTS":
      {
        return updateAllBreakpoints(state, action);
      }

    case "SET_BREAKPOINT_CONDITION":
      {
        return updateBreakpoint(state, action);
      }

    case "REMOVE_BREAKPOINT":
      {
        if (action.breakpoint.hidden) {
          return state;
        }

        return removeBreakpoint(state, action);
      }
  }

  return state;
}

function addBreakpoint(state, action) {
  if (action.breakpoint.hidden || action.status !== "done") {
    return state;
  } // when the action completes, we can commit the breakpoint


  const {
    breakpoint
  } = action.value;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return { ...state,
    [locationId]: pendingBreakpoint
  };
}

function syncBreakpoint(state, action) {
  const {
    breakpoint,
    previousLocation
  } = action;

  if (previousLocation) {
    const previousLocationId = (0, _breakpoint.makePendingLocationId)(previousLocation);
    state = deleteBreakpoint(state, previousLocationId);
  }

  if (!breakpoint) {
    return state;
  }

  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return { ...state,
    [locationId]: pendingBreakpoint
  };
}

function updateBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return { ...state,
    [locationId]: pendingBreakpoint
  };
}

function updateAllBreakpoints(state, action) {
  const {
    breakpoints
  } = action;
  breakpoints.forEach(breakpoint => {
    const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
    const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
    state = { ...state,
      [locationId]: pendingBreakpoint
    };
  });
  return state;
}

function removeBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBp = state[locationId];

  if (!pendingBp && action.status == "start") {
    return {};
  }

  return deleteBreakpoint(state, locationId);
}

function deleteBreakpoint(state, locationId) {
  state = { ...state
  };
  delete state[locationId];
  return state;
} // Selectors
// TODO: these functions should be moved out of the reducer


function getPendingBreakpoints(state) {
  return state.pendingBreakpoints;
}

function getPendingBreakpointList(state) {
  return Object.values(getPendingBreakpoints(state));
}

function getPendingBreakpointsForSource(state, sourceUrl) {
  return getPendingBreakpointList(state).filter(pendingBreakpoint => pendingBreakpoint.location.sourceUrl === sourceUrl);
}

exports.default = update;