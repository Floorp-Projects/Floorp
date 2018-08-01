"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initialPendingBreakpointsState = initialPendingBreakpointsState;
exports.getPendingBreakpoints = getPendingBreakpoints;
exports.getPendingBreakpointsForSource = getPendingBreakpointsForSource;

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _breakpoint = require("../utils/breakpoint/index");

var _prefs = require("../utils/prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Pending breakpoints reducer
 * @module reducers/pending-breakpoints
 */
function initialPendingBreakpointsState() {
  return (0, _makeRecord2.default)({
    pendingBreakpoints: restorePendingBreakpoints()
  })();
}

function update(state = initialPendingBreakpointsState(), action) {
  switch (action.type) {
    case "ADD_BREAKPOINT":
      {
        if (action.breakpoint.hidden) {
          return state;
        }

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
  if (action.status !== "done") {
    return state;
  } // when the action completes, we can commit the breakpoint


  const {
    breakpoint
  } = action.value;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return state.setIn(["pendingBreakpoints", locationId], pendingBreakpoint);
}

function syncBreakpoint(state, action) {
  const {
    breakpoint,
    previousLocation
  } = action;

  if (previousLocation) {
    state = state.deleteIn(["pendingBreakpoints", (0, _breakpoint.makePendingLocationId)(previousLocation)]);
  }

  if (!breakpoint) {
    return state;
  }

  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return state.setIn(["pendingBreakpoints", locationId], pendingBreakpoint);
}

function updateBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBreakpoint = (0, _breakpoint.createPendingBreakpoint)(breakpoint);
  return state.setIn(["pendingBreakpoints", locationId], pendingBreakpoint);
}

function updateAllBreakpoints(state, action) {
  const {
    breakpoints
  } = action;
  breakpoints.forEach(breakpoint => {
    const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
    state = state.setIn(["pendingBreakpoints", locationId], breakpoint);
  });
  return state;
}

function removeBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const locationId = (0, _breakpoint.makePendingLocationId)(breakpoint.location);
  const pendingBp = state.pendingBreakpoints.get(locationId);

  if (!pendingBp && action.status == "start") {
    return state.set("pendingBreakpoints", I.Map());
  }

  return state.deleteIn(["pendingBreakpoints", locationId]);
} // Selectors
// TODO: these functions should be moved out of the reducer


function getPendingBreakpoints(state) {
  return state.pendingBreakpoints.pendingBreakpoints;
}

function getPendingBreakpointsForSource(state, sourceUrl) {
  const pendingBreakpoints = state.pendingBreakpoints.pendingBreakpoints || I.Map();
  return pendingBreakpoints.filter(pendingBreakpoint => pendingBreakpoint.location.sourceUrl === sourceUrl);
}

function restorePendingBreakpoints() {
  return I.Map(_prefs.prefs.pendingBreakpoints);
}

exports.default = update;