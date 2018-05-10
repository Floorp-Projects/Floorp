"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initialBreakpointsState = initialBreakpointsState;
exports.getBreakpoints = getBreakpoints;
exports.getBreakpoint = getBreakpoint;
exports.getBreakpointsDisabled = getBreakpointsDisabled;
exports.getBreakpointsLoading = getBreakpointsLoading;
exports.getBreakpointsForSource = getBreakpointsForSource;
exports.getBreakpointForLine = getBreakpointForLine;
exports.getHiddenBreakpoint = getHiddenBreakpoint;
exports.getHiddenBreakpointLocation = getHiddenBreakpointLocation;

var _immutable = require("devtools/client/shared/vendor/immutable");

var I = _interopRequireWildcard(_immutable);

var _makeRecord = require("../utils/makeRecord");

var _makeRecord2 = _interopRequireDefault(_makeRecord);

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _breakpoint = require("../utils/breakpoint/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

function initialBreakpointsState() {
  return (0, _makeRecord2.default)({
    breakpoints: I.Map(),
    breakpointsDisabled: false
  })();
}

function update(state = initialBreakpointsState(), action) {
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
        return removeBreakpoint(state, action);
      }

    case "REMAP_BREAKPOINTS":
      {
        return remapBreakpoints(state, action);
      }

    case "NAVIGATE":
      {
        return initialBreakpointsState();
      }
  }

  return state;
}

function addBreakpoint(state, action) {
  if (action.status === "start" && action.breakpoint) {
    const {
      breakpoint
    } = action;
    const locationId = (0, _breakpoint.makeLocationId)(breakpoint.location);
    return state.setIn(["breakpoints", locationId], breakpoint);
  } // when the action completes, we can commit the breakpoint


  if (action.status === "done") {
    const {
      value
    } = action;
    return syncBreakpoint(state, value);
  } // Remove the optimistic update


  if (action.status === "error" && action.breakpoint) {
    const locationId = (0, _breakpoint.makeLocationId)(action.breakpoint.location);
    return state.deleteIn(["breakpoints", locationId]);
  }

  return state;
}

function syncBreakpoint(state, data) {
  const {
    breakpoint,
    previousLocation
  } = data;

  if (previousLocation) {
    state = state.deleteIn(["breakpoints", (0, _breakpoint.makeLocationId)(previousLocation)]);
  }

  if (!breakpoint) {
    return state;
  }

  const locationId = (0, _breakpoint.makeLocationId)(breakpoint.location);
  return state.setIn(["breakpoints", locationId], breakpoint);
}

function updateBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const locationId = (0, _breakpoint.makeLocationId)(breakpoint.location);
  return state.setIn(["breakpoints", locationId], breakpoint);
}

function updateAllBreakpoints(state, action) {
  const {
    breakpoints
  } = action;
  breakpoints.forEach(breakpoint => {
    const locationId = (0, _breakpoint.makeLocationId)(breakpoint.location);
    state = state.setIn(["breakpoints", locationId], breakpoint);
  });
  return state;
}

function remapBreakpoints(state, action) {
  const breakpoints = action.breakpoints.reduce((updatedBreakpoints, breakpoint) => {
    const locationId = (0, _breakpoint.makeLocationId)(breakpoint.location);
    return _objectSpread({}, updatedBreakpoints, {
      [locationId]: breakpoint
    });
  }, {});
  return state.set("breakpoints", I.Map(breakpoints));
}

function removeBreakpoint(state, action) {
  const {
    breakpoint
  } = action;
  const id = (0, _breakpoint.makeLocationId)(breakpoint.location);
  return state.deleteIn(["breakpoints", id]);
} // Selectors
// TODO: these functions should be moved out of the reducer


function getBreakpoints(state) {
  return state.breakpoints.breakpoints;
}

function getBreakpoint(state, location) {
  const breakpoints = getBreakpoints(state);
  return breakpoints.get((0, _breakpoint.makeLocationId)(location));
}

function getBreakpointsDisabled(state) {
  return state.breakpoints.breakpoints.every(x => x.disabled);
}

function getBreakpointsLoading(state) {
  const breakpoints = getBreakpoints(state);
  const isLoading = !!breakpoints.valueSeq().filter(bp => bp.loading).first();
  return breakpoints.size > 0 && isLoading;
}

function getBreakpointsForSource(state, sourceId) {
  if (!sourceId) {
    return I.Map();
  }

  const isGeneratedSource = (0, _devtoolsSourceMap.isGeneratedId)(sourceId);
  const breakpoints = getBreakpoints(state);
  return breakpoints.filter(bp => {
    const location = isGeneratedSource ? bp.generatedLocation || bp.location : bp.location;
    return location.sourceId === sourceId;
  });
}

function getBreakpointForLine(state, sourceId, line) {
  if (!sourceId) {
    return I.Map();
  }

  const breakpoints = getBreakpointsForSource(state, sourceId);
  return breakpoints.find(breakpoint => breakpoint.location.line === line);
}

function getHiddenBreakpoint(state) {
  return getBreakpoints(state).valueSeq().filter(breakpoint => breakpoint.hidden).first();
}

function getHiddenBreakpointLocation(state) {
  const hiddenBreakpoint = getHiddenBreakpoint(state);

  if (!hiddenBreakpoint) {
    return null;
  }

  return hiddenBreakpoint.location;
}

exports.default = update;