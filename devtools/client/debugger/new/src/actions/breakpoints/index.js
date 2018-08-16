"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.syncBreakpoint = exports.enableBreakpoint = exports.addHiddenBreakpoint = exports.addBreakpoint = undefined;
exports.removeBreakpoint = removeBreakpoint;
exports.disableBreakpoint = disableBreakpoint;
exports.toggleAllBreakpoints = toggleAllBreakpoints;
exports.toggleBreakpoints = toggleBreakpoints;
exports.removeAllBreakpoints = removeAllBreakpoints;
exports.removeBreakpoints = removeBreakpoints;
exports.remapBreakpoints = remapBreakpoints;
exports.setBreakpointCondition = setBreakpointCondition;
exports.toggleBreakpoint = toggleBreakpoint;
exports.toggleBreakpointsAtLine = toggleBreakpointsAtLine;
exports.addOrToggleDisabledBreakpoint = addOrToggleDisabledBreakpoint;
exports.toggleDisabledBreakpoint = toggleDisabledBreakpoint;

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _promise = require("../utils/middleware/promise");

var _selectors = require("../../selectors/index");

var _breakpoint = require("../../utils/breakpoint/index");

var _addBreakpoint = require("./addBreakpoint");

var _remapLocations = require("./remapLocations");

var _remapLocations2 = _interopRequireDefault(_remapLocations);

var _syncBreakpoint = require("./syncBreakpoint");

var _ast = require("../../reducers/ast");

var _telemetry = require("../../utils/telemetry");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for breakpoints
 * @module actions/breakpoints
 */

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
function removeBreakpoint(location) {
  return ({
    dispatch,
    getState,
    client
  }) => {
    const bp = (0, _selectors.getBreakpoint)(getState(), location);

    if (!bp || bp.loading) {
      return;
    }

    (0, _telemetry.recordEvent)("remove_breakpoint"); // If the breakpoint is already disabled, we don't need to communicate
    // with the server. We just need to dispatch an action
    // simulating a successful server request

    if (bp.disabled) {
      return dispatch({
        type: "REMOVE_BREAKPOINT",
        breakpoint: bp,
        status: "done"
      });
    }

    return dispatch({
      type: "REMOVE_BREAKPOINT",
      breakpoint: bp,
      disabled: false,
      [_promise.PROMISE]: client.removeBreakpoint(bp.generatedLocation)
    });
  };
}
/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */


function disableBreakpoint(location) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    const bp = (0, _selectors.getBreakpoint)(getState(), location);

    if (!bp || bp.loading) {
      return;
    }

    await client.removeBreakpoint(bp.generatedLocation);
    const newBreakpoint = { ...bp,
      disabled: true
    };
    return dispatch({
      type: "DISABLE_BREAKPOINT",
      breakpoint: newBreakpoint
    });
  };
}
/**
 * Toggle All Breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */


function toggleAllBreakpoints(shouldDisableBreakpoints) {
  return async ({
    dispatch,
    getState,
    client
  }) => {
    const breakpoints = (0, _selectors.getBreakpoints)(getState());
    const modifiedBreakpoints = [];

    for (const [, breakpoint] of breakpoints) {
      if (shouldDisableBreakpoints) {
        await client.removeBreakpoint(breakpoint.generatedLocation);
        const newBreakpoint = { ...breakpoint,
          disabled: true
        };
        modifiedBreakpoints.push(newBreakpoint);
      } else {
        const newBreakpoint = { ...breakpoint,
          disabled: false
        };
        modifiedBreakpoints.push(newBreakpoint);
      }
    }

    if (shouldDisableBreakpoints) {
      return dispatch({
        type: "DISABLE_ALL_BREAKPOINTS",
        breakpoints: modifiedBreakpoints
      });
    }

    return dispatch({
      type: "ENABLE_ALL_BREAKPOINTS",
      breakpoints: modifiedBreakpoints
    });
  };
}
/**
 * Toggle Breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */


function toggleBreakpoints(shouldDisableBreakpoints, breakpoints) {
  return async ({
    dispatch
  }) => {
    const promises = breakpoints.valueSeq().toJS().map(([, breakpoint]) => shouldDisableBreakpoints ? dispatch(disableBreakpoint(breakpoint.location)) : dispatch((0, _addBreakpoint.enableBreakpoint)(breakpoint.location)));
    await Promise.all(promises);
  };
}
/**
 * Removes all breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */


function removeAllBreakpoints() {
  return async ({
    dispatch,
    getState
  }) => {
    const breakpointList = (0, _selectors.getBreakpoints)(getState()).valueSeq().toJS();
    return Promise.all(breakpointList.map(bp => dispatch(removeBreakpoint(bp.location))));
  };
}
/**
 * Removes breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */


function removeBreakpoints(breakpoints) {
  return async ({
    dispatch
  }) => {
    const breakpointList = breakpoints.valueSeq().toJS();
    return Promise.all(breakpointList.map(bp => dispatch(removeBreakpoint(bp.location))));
  };
}

function remapBreakpoints(sourceId) {
  return async ({
    dispatch,
    getState,
    sourceMaps
  }) => {
    const breakpoints = (0, _selectors.getBreakpoints)(getState());
    const newBreakpoints = await (0, _remapLocations2.default)(breakpoints, sourceId, sourceMaps);
    return dispatch({
      type: "REMAP_BREAKPOINTS",
      breakpoints: newBreakpoints
    });
  };
}
/**
 * Update the condition of a breakpoint.
 *
 * @throws {Error} "not implemented"
 * @memberof actions/breakpoints
 * @static
 * @param {Location} location
 *        @see DebuggerController.Breakpoints.addBreakpoint
 * @param {string} condition
 *        The condition to set on the breakpoint
 * @param {Boolean} $1.disabled Disable value for breakpoint value
 */


function setBreakpointCondition(location, {
  condition
} = {}) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const bp = (0, _selectors.getBreakpoint)(getState(), location);

    if (!bp) {
      return dispatch((0, _addBreakpoint.addBreakpoint)(location, {
        condition
      }));
    }

    if (bp.loading) {
      return;
    }

    if (bp.disabled) {
      await dispatch((0, _addBreakpoint.enableBreakpoint)(location));
      bp.disabled = !bp.disabled;
    }

    await client.setBreakpointCondition(bp.id, location, condition, (0, _devtoolsSourceMap.isOriginalId)(bp.location.sourceId));
    const newBreakpoint = { ...bp,
      condition
    };
    (0, _breakpoint.assertBreakpoint)(newBreakpoint);
    return dispatch({
      type: "SET_BREAKPOINT_CONDITION",
      breakpoint: newBreakpoint
    });
  };
}

function toggleBreakpoint(line, column) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const state = getState();
    const selectedSource = (0, _selectors.getSelectedSource)(state);

    if (!line || !selectedSource) {
      return;
    }

    const bp = (0, _selectors.getBreakpointAtLocation)(state, {
      line,
      column
    });
    const isEmptyLine = (0, _ast.isEmptyLineInSource)(state, line, selectedSource.id);

    if (!bp && isEmptyLine || bp && bp.loading) {
      return;
    }

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      return dispatch(removeBreakpoint({
        sourceId: bp.location.sourceId,
        sourceUrl: bp.location.sourceUrl,
        line: bp.location.line,
        column: column || bp.location.column
      }));
    }

    return dispatch((0, _addBreakpoint.addBreakpoint)({
      sourceId: selectedSource.id,
      sourceUrl: selectedSource.url,
      line: line,
      column: column
    }));
  };
}

function toggleBreakpointsAtLine(line, column) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const state = getState();
    const selectedSource = (0, _selectors.getSelectedSource)(state);

    if (!line || !selectedSource) {
      return;
    }

    const bps = (0, _selectors.getBreakpointsAtLine)(state, line);
    const isEmptyLine = (0, _ast.isEmptyLineInSource)(state, line, selectedSource.id);

    if (bps.size === 0 && !isEmptyLine) {
      return dispatch((0, _addBreakpoint.addBreakpoint)({
        sourceId: selectedSource.id,
        sourceUrl: selectedSource.url,
        line,
        column
      }));
    }

    return Promise.all(bps.map(bp => dispatch(removeBreakpoint(bp.location))));
  };
}

function addOrToggleDisabledBreakpoint(line, column) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const selectedSource = (0, _selectors.getSelectedSource)(getState());

    if (!line || !selectedSource) {
      return;
    }

    const bp = (0, _selectors.getBreakpointAtLocation)(getState(), {
      line,
      column
    });

    if (bp && bp.loading) {
      return;
    }

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      return dispatch(toggleDisabledBreakpoint(line, column || bp.location.column));
    }

    return dispatch((0, _addBreakpoint.addBreakpoint)({
      sourceId: selectedSource.id,
      sourceUrl: selectedSource.url,
      line: line,
      column: column
    }));
  };
}

function toggleDisabledBreakpoint(line, column) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const bp = (0, _selectors.getBreakpointAtLocation)(getState(), {
      line,
      column
    });

    if (!bp || bp.loading) {
      return;
    }

    if (!bp.disabled) {
      return dispatch(disableBreakpoint(bp.location));
    }

    return dispatch((0, _addBreakpoint.enableBreakpoint)(bp.location));
  };
}

exports.addBreakpoint = _addBreakpoint.addBreakpoint;
exports.addHiddenBreakpoint = _addBreakpoint.addHiddenBreakpoint;
exports.enableBreakpoint = _addBreakpoint.enableBreakpoint;
exports.syncBreakpoint = _syncBreakpoint.syncBreakpoint;