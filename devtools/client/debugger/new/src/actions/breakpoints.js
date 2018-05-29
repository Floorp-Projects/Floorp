"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.syncBreakpoint = syncBreakpoint;
exports.addBreakpoint = addBreakpoint;
exports.addHiddenBreakpoint = addHiddenBreakpoint;
exports.removeBreakpoint = removeBreakpoint;
exports.enableBreakpoint = enableBreakpoint;
exports.disableBreakpoint = disableBreakpoint;
exports.toggleAllBreakpoints = toggleAllBreakpoints;
exports.toggleBreakpoints = toggleBreakpoints;
exports.removeAllBreakpoints = removeAllBreakpoints;
exports.removeBreakpoints = removeBreakpoints;
exports.remapBreakpoints = remapBreakpoints;
exports.setBreakpointCondition = setBreakpointCondition;
exports.toggleBreakpoint = toggleBreakpoint;
exports.addOrToggleDisabledBreakpoint = addOrToggleDisabledBreakpoint;
exports.toggleDisabledBreakpoint = toggleDisabledBreakpoint;

var _promise = require("./utils/middleware/promise");

var _selectors = require("../selectors/index");

var _breakpoint = require("../utils/breakpoint/index");

var _addBreakpoint = require("./breakpoints/addBreakpoint");

var _addBreakpoint2 = _interopRequireDefault(_addBreakpoint);

var _remapLocations = require("./breakpoints/remapLocations");

var _remapLocations2 = _interopRequireDefault(_remapLocations);

var _ast = require("../reducers/ast");

var _syncBreakpoint = require("./breakpoints/syncBreakpoint");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

/**
 * Syncing a breakpoint add breakpoint information that is stored, and
 * contact the server for more data.
 *
 * @memberof actions/breakpoints
 * @static
 * @param {String} $1.sourceId String  value
 * @param {PendingBreakpoint} $1.location PendingBreakpoint  value
 */
function syncBreakpoint(sourceId, pendingBreakpoint) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const {
      breakpoint,
      previousLocation
    } = await (0, _syncBreakpoint.syncClientBreakpoint)(getState, client, sourceMaps, sourceId, pendingBreakpoint);
    return dispatch({
      type: "SYNC_BREAKPOINT",
      breakpoint,
      previousLocation
    });
  };
}
/**
 * Add a new breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 * @param {String} $1.condition Conditional breakpoint condition value
 * @param {Boolean} $1.disabled Disable value for breakpoint value
 */


function addBreakpoint(location, {
  condition,
  hidden
} = {}) {
  const breakpoint = (0, _breakpoint.createBreakpoint)(location, {
    condition,
    hidden
  });
  return ({
    dispatch,
    getState,
    sourceMaps,
    client
  }) => {
    return dispatch({
      type: "ADD_BREAKPOINT",
      breakpoint,
      [_promise.PROMISE]: (0, _addBreakpoint2.default)(getState, client, sourceMaps, breakpoint)
    });
  };
}
/**
 * Add a new hidden breakpoint
 *
 * @memberOf actions/breakpoints
 * @param location
 * @return {function(ThunkArgs)}
 */


function addHiddenBreakpoint(location) {
  return ({
    dispatch
  }) => {
    return dispatch(addBreakpoint(location, {
      hidden: true
    }));
  };
}
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
    } // If the breakpoint is already disabled, we don't need to communicate
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
 * Enabling a breakpoint
 * will reuse the existing breakpoint information that is stored.
 *
 * @memberof actions/breakpoints
 * @static
 * @param {Location} $1.location Location  value
 */


function enableBreakpoint(location) {
  return async ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    const breakpoint = (0, _selectors.getBreakpoint)(getState(), location);

    if (!breakpoint || breakpoint.loading) {
      return;
    } // To instantly reflect in the UI, we optimistically enable the breakpoint


    const enabledBreakpoint = _objectSpread({}, breakpoint, {
      disabled: false
    });

    return dispatch({
      type: "ENABLE_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      [_promise.PROMISE]: (0, _addBreakpoint2.default)(getState, client, sourceMaps, breakpoint)
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

    const newBreakpoint = _objectSpread({}, bp, {
      disabled: true
    });

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

        const newBreakpoint = _objectSpread({}, breakpoint, {
          disabled: true
        });

        modifiedBreakpoints.push(newBreakpoint);
      } else {
        const newBreakpoint = _objectSpread({}, breakpoint, {
          disabled: false
        });

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
    for (const [, breakpoint] of breakpoints) {
      if (shouldDisableBreakpoints) {
        await dispatch(disableBreakpoint(breakpoint.location));
      } else {
        await dispatch(enableBreakpoint(breakpoint.location));
      }
    }
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
    const breakpoints = (0, _selectors.getBreakpoints)(getState());

    for (const [, breakpoint] of breakpoints) {
      await dispatch(removeBreakpoint(breakpoint.location));
    }
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
    for (const [, breakpoint] of breakpoints) {
      await dispatch(removeBreakpoint(breakpoint.location));
    }
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
      return dispatch(addBreakpoint(location, {
        condition
      }));
    }

    if (bp.loading) {
      return;
    }

    if (bp.disabled) {
      await dispatch(enableBreakpoint(location));
      bp.disabled = !bp.disabled;
    }

    await client.setBreakpointCondition(bp.id, location, condition, sourceMaps.isOriginalId(bp.location.sourceId));

    const newBreakpoint = _objectSpread({}, bp, {
      condition
    });

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
    if (!line) {
      return;
    }

    const state = getState();
    const selectedSource = (0, _selectors.getSelectedSource)(state);
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

    return dispatch(addBreakpoint({
      sourceId: selectedSource.id,
      sourceUrl: selectedSource.url,
      line: line,
      column: column
    }));
  };
}

function addOrToggleDisabledBreakpoint(line, column) {
  return ({
    dispatch,
    getState,
    client,
    sourceMaps
  }) => {
    if (!line) {
      return;
    }

    const selectedSource = (0, _selectors.getSelectedSource)(getState());
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

    return dispatch(addBreakpoint({
      sourceId: selectedSource.get("id"),
      sourceUrl: selectedSource.get("url"),
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

    return dispatch(enableBreakpoint(bp.location));
  };
}