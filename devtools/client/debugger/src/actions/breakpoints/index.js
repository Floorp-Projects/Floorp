/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Redux actions for breakpoints
 * @module actions/breakpoints
 */

import { PROMISE } from "../utils/middleware/promise";
import { asyncStore } from "../../utils/prefs";
import { createLocation } from "../../utils/location";
import {
  getBreakpointsList,
  getXHRBreakpoints,
  getSelectedSource,
  getBreakpointAtLocation,
  getBreakpointsForSource,
  getBreakpointsAtLine,
} from "../../selectors/index";
import { createXHRBreakpoint } from "../../utils/breakpoint/index";
import {
  addBreakpoint,
  removeBreakpoint,
  enableBreakpoint,
  disableBreakpoint,
} from "./modify";
import { getOriginalLocation } from "../../utils/source-maps";

export * from "./breakpointPositions";
export * from "./modify";
export * from "./syncBreakpoint";

export function addHiddenBreakpoint(location) {
  return ({ dispatch }) => {
    return dispatch(addBreakpoint(location, { hidden: true }));
  };
}

/**
 * Disable all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpointsInSource(source) {
  return async ({ dispatch, getState, client }) => {
    const breakpoints = getBreakpointsForSource(getState(), source);
    for (const breakpoint of breakpoints) {
      if (!breakpoint.disabled) {
        dispatch(disableBreakpoint(breakpoint));
      }
    }
  };
}

/**
 * Enable all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function enableBreakpointsInSource(source) {
  return async ({ dispatch, getState, client }) => {
    const breakpoints = getBreakpointsForSource(getState(), source);
    for (const breakpoint of breakpoints) {
      if (breakpoint.disabled) {
        dispatch(enableBreakpoint(breakpoint));
      }
    }
  };
}

/**
 * Toggle All Breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function toggleAllBreakpoints(shouldDisableBreakpoints) {
  return async ({ dispatch, getState, client }) => {
    const breakpoints = getBreakpointsList(getState());

    for (const breakpoint of breakpoints) {
      if (shouldDisableBreakpoints) {
        dispatch(disableBreakpoint(breakpoint));
      } else {
        dispatch(enableBreakpoint(breakpoint));
      }
    }
  };
}

/**
 * Toggle Breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function toggleBreakpoints(shouldDisableBreakpoints, breakpoints) {
  return async ({ dispatch }) => {
    const promises = breakpoints.map(breakpoint =>
      shouldDisableBreakpoints
        ? dispatch(disableBreakpoint(breakpoint))
        : dispatch(enableBreakpoint(breakpoint))
    );

    await Promise.all(promises);
  };
}

export function toggleBreakpointsAtLine(shouldDisableBreakpoints, line) {
  return async ({ dispatch, getState }) => {
    const breakpoints = getBreakpointsAtLine(getState(), line);
    return dispatch(toggleBreakpoints(shouldDisableBreakpoints, breakpoints));
  };
}

/**
 * Removes all breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeAllBreakpoints() {
  return async ({ dispatch, getState }) => {
    const breakpointList = getBreakpointsList(getState());
    await Promise.all(breakpointList.map(bp => dispatch(removeBreakpoint(bp))));
    dispatch({ type: "CLEAR_BREAKPOINTS" });
  };
}

/**
 * Removes breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoints(breakpoints) {
  return async ({ dispatch }) => {
    return Promise.all(breakpoints.map(bp => dispatch(removeBreakpoint(bp))));
  };
}

/**
 * Removes all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpointsInSource(source) {
  return async ({ dispatch, getState, client }) => {
    const breakpoints = getBreakpointsForSource(getState(), source);
    for (const breakpoint of breakpoints) {
      dispatch(removeBreakpoint(breakpoint));
    }
  };
}

/**
 * Update the original location information of breakpoints.

/*
 * Update breakpoints for a source that just got pretty printed.
 * This method maps the breakpoints currently set only against the
 * non-pretty-printed (generated) source to the related pretty-printed
 * (original) source by querying the SourceMap service.
 *
 * @param {String} source - the generated source
 */
export function updateBreakpointsForNewPrettyPrintedSource(source) {
  return async thunkArgs => {
    const { dispatch, getState } = thunkArgs;
    if (source.isOriginal) {
      console.error("Can't update breakpoints on original sources");
      return;
    }
    const breakpoints = getBreakpointsForSource(getState(), source);
    // Remap the breakpoints with the original location information from
    // the pretty-printed source.
    const newBreakpoints = await Promise.all(
      breakpoints.map(async breakpoint => {
        const location = await getOriginalLocation(
          breakpoint.generatedLocation,
          thunkArgs
        );
        return { ...breakpoint, location };
      })
    );

    // Normally old breakpoints will be clobbered if we re-add them, but when
    // remapping we have changed the source maps and the old breakpoints will
    // have different locations than the new ones. Manually remove the
    // old breakpoints before adding the new ones.
    for (const bp of breakpoints) {
      dispatch(removeBreakpoint(bp));
    }

    for (const bp of newBreakpoints) {
      await dispatch(addBreakpoint(bp.location, bp.options, bp.disabled));
    }
  };
}

export function toggleBreakpointAtLine(line) {
  return ({ dispatch, getState }) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);

    if (!selectedSource) {
      return null;
    }

    const bp = getBreakpointAtLocation(state, { line, column: undefined });
    if (bp) {
      return dispatch(removeBreakpoint(bp));
    }
    return dispatch(
      addBreakpoint(
        createLocation({
          source: selectedSource,
          line,
        })
      )
    );
  };
}

export function addBreakpointAtLine(line, shouldLog = false, disabled = false) {
  return ({ dispatch, getState }) => {
    const state = getState();
    const source = getSelectedSource(state);

    if (!source) {
      return null;
    }
    const breakpointLocation = createLocation({
      source,
      column: undefined,
      line,
    });

    const options = {};
    if (shouldLog) {
      options.logValue = "displayName";
    }

    return dispatch(addBreakpoint(breakpointLocation, options, disabled));
  };
}

export function removeBreakpointsAtLine(source, line) {
  return ({ dispatch, getState }) => {
    const breakpointsAtLine = getBreakpointsForSource(getState(), source, line);
    return dispatch(removeBreakpoints(breakpointsAtLine));
  };
}

export function disableBreakpointsAtLine(source, line) {
  return ({ dispatch, getState }) => {
    const breakpointsAtLine = getBreakpointsForSource(getState(), source, line);
    return dispatch(toggleBreakpoints(true, breakpointsAtLine));
  };
}

export function enableBreakpointsAtLine(source, line) {
  return ({ dispatch, getState }) => {
    const breakpointsAtLine = getBreakpointsForSource(getState(), source, line);
    return dispatch(toggleBreakpoints(false, breakpointsAtLine));
  };
}

export function toggleDisabledBreakpoint(breakpoint) {
  return ({ dispatch, getState }) => {
    if (!breakpoint.disabled) {
      return dispatch(disableBreakpoint(breakpoint));
    }
    return dispatch(enableBreakpoint(breakpoint));
  };
}

export function enableXHRBreakpoint(index, bp) {
  return ({ dispatch, getState, client }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = bp || xhrBreakpoints[index];
    const enabledBreakpoint = {
      ...breakpoint,
      disabled: false,
    };

    return dispatch({
      type: "ENABLE_XHR_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      index,
      [PROMISE]: client.setXHRBreakpoint(breakpoint.path, breakpoint.method),
    });
  };
}

export function disableXHRBreakpoint(index, bp) {
  return ({ dispatch, getState, client }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = bp || xhrBreakpoints[index];
    const disabledBreakpoint = {
      ...breakpoint,
      disabled: true,
    };

    return dispatch({
      type: "DISABLE_XHR_BREAKPOINT",
      breakpoint: disabledBreakpoint,
      index,
      [PROMISE]: client.removeXHRBreakpoint(breakpoint.path, breakpoint.method),
    });
  };
}

export function updateXHRBreakpoint(index, path, method) {
  return ({ dispatch, getState, client }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = xhrBreakpoints[index];

    const updatedBreakpoint = {
      ...breakpoint,
      path,
      method,
      text: L10N.getFormatStr("xhrBreakpoints.item.label", path),
    };

    return dispatch({
      type: "UPDATE_XHR_BREAKPOINT",
      breakpoint: updatedBreakpoint,
      index,
      [PROMISE]: Promise.all([
        client.removeXHRBreakpoint(breakpoint.path, breakpoint.method),
        client.setXHRBreakpoint(path, method),
      ]),
    });
  };
}
export function togglePauseOnAny() {
  return ({ dispatch, getState }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const index = xhrBreakpoints.findIndex(({ path }) => path.length === 0);
    if (index < 0) {
      return dispatch(setXHRBreakpoint("", "ANY"));
    }

    const bp = xhrBreakpoints[index];
    if (bp.disabled) {
      return dispatch(enableXHRBreakpoint(index, bp));
    }

    return dispatch(disableXHRBreakpoint(index, bp));
  };
}

export function setXHRBreakpoint(path, method) {
  return ({ dispatch, getState, client }) => {
    const breakpoint = createXHRBreakpoint(path, method);

    return dispatch({
      type: "SET_XHR_BREAKPOINT",
      breakpoint,
      [PROMISE]: client.setXHRBreakpoint(path, method),
    });
  };
}

export function removeAllXHRBreakpoints() {
  return async ({ dispatch, getState, client }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const promises = xhrBreakpoints.map(breakpoint =>
      client.removeXHRBreakpoint(breakpoint.path, breakpoint.method)
    );
    await dispatch({
      type: "CLEAR_XHR_BREAKPOINTS",
      [PROMISE]: Promise.all(promises),
    });
    asyncStore.xhrBreakpoints = [];
  };
}

export function removeXHRBreakpoint(index) {
  return ({ dispatch, getState, client }) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = xhrBreakpoints[index];
    return dispatch({
      type: "REMOVE_XHR_BREAKPOINT",
      breakpoint,
      index,
      [PROMISE]: client.removeXHRBreakpoint(breakpoint.path, breakpoint.method),
    });
  };
}
