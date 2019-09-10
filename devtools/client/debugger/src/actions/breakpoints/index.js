/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for breakpoints
 * @module actions/breakpoints
 */

import { PROMISE } from "../utils/middleware/promise";
import {
  getBreakpointsList,
  getXHRBreakpoints,
  getSelectedSource,
  getBreakpointAtLocation,
  getBreakpointsForSource,
  getBreakpointsAtLine,
} from "../../selectors";
import { createXHRBreakpoint } from "../../utils/breakpoint";
import {
  addBreakpoint,
  removeBreakpoint,
  enableBreakpoint,
  disableBreakpoint,
} from "./modify";
import remapLocations from "./remapLocations";

// this will need to be changed so that addCLientBreakpoint is removed

import type { ThunkArgs } from "../types";
import type {
  Breakpoint,
  Source,
  SourceLocation,
  XHRBreakpoint,
  Context,
} from "../../types";

export * from "./breakpointPositions";
export * from "./modify";
export * from "./syncBreakpoint";

export function addHiddenBreakpoint(cx: Context, location: SourceLocation) {
  return ({ dispatch }: ThunkArgs) => {
    return dispatch(addBreakpoint(cx, location, { hidden: true }));
  };
}

/**
 * Disable all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpointsInSource(cx: Context, source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const breakpoint of breakpoints) {
      if (!breakpoint.disabled) {
        dispatch(disableBreakpoint(cx, breakpoint));
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
export function enableBreakpointsInSource(cx: Context, source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const breakpoint of breakpoints) {
      if (breakpoint.disabled) {
        dispatch(enableBreakpoint(cx, breakpoint));
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
export function toggleAllBreakpoints(
  cx: Context,
  shouldDisableBreakpoints: boolean
) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsList(getState());

    for (const breakpoint of breakpoints) {
      if (shouldDisableBreakpoints) {
        dispatch(disableBreakpoint(cx, breakpoint));
      } else {
        dispatch(enableBreakpoint(cx, breakpoint));
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
export function toggleBreakpoints(
  cx: Context,
  shouldDisableBreakpoints: boolean,
  breakpoints: Breakpoint[]
) {
  return async ({ dispatch }: ThunkArgs) => {
    const promises = breakpoints.map(breakpoint =>
      shouldDisableBreakpoints
        ? dispatch(disableBreakpoint(cx, breakpoint))
        : dispatch(enableBreakpoint(cx, breakpoint))
    );

    await Promise.all(promises);
  };
}

export function toggleBreakpointsAtLine(
  cx: Context,
  shouldDisableBreakpoints: boolean,
  line: number
) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const breakpoints = getBreakpointsAtLine(getState(), line);
    return dispatch(
      toggleBreakpoints(cx, shouldDisableBreakpoints, breakpoints)
    );
  };
}

/**
 * Removes all breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeAllBreakpoints(cx: Context) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const breakpointList = getBreakpointsList(getState());
    await Promise.all(
      breakpointList.map(bp => dispatch(removeBreakpoint(cx, bp)))
    );
    dispatch({ type: "REMOVE_BREAKPOINTS" });
  };
}

/**
 * Removes breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoints(cx: Context, breakpoints: Breakpoint[]) {
  return async ({ dispatch }: ThunkArgs) => {
    return Promise.all(
      breakpoints.map(bp => dispatch(removeBreakpoint(cx, bp)))
    );
  };
}

/**
 * Removes all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpointsInSource(cx: Context, source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const breakpoint of breakpoints) {
      dispatch(removeBreakpoint(cx, breakpoint));
    }
  };
}

export function remapBreakpoints(cx: Context, sourceId: string) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), sourceId);
    const newBreakpoints = await remapLocations(
      breakpoints,
      sourceId,
      sourceMaps
    );

    // Normally old breakpoints will be clobbered if we re-add them, but when
    // remapping we have changed the source maps and the old breakpoints will
    // have different locations than the new ones. Manually remove the
    // old breakpoints before adding the new ones.
    for (const bp of breakpoints) {
      dispatch(removeBreakpoint(cx, bp));
    }

    for (const bp of newBreakpoints) {
      await dispatch(addBreakpoint(cx, bp.location, bp.options, bp.disabled));
    }
  };
}

export function toggleBreakpointAtLine(cx: Context, line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);

    if (!selectedSource) {
      return;
    }

    const bp = getBreakpointAtLocation(state, { line, column: undefined });
    if (bp) {
      return dispatch(removeBreakpoint(cx, bp));
    }
    return dispatch(
      addBreakpoint(cx, {
        sourceId: selectedSource.id,
        sourceUrl: selectedSource.url,
        line: line,
      })
    );
  };
}

export function addBreakpointAtLine(
  cx: Context,
  line: number,
  shouldLog: boolean = false,
  disabled: boolean = false
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const source = getSelectedSource(state);

    if (!source) {
      return;
    }
    const breakpointLocation = {
      sourceId: source.id,
      sourceUrl: source.url,
      column: undefined,
      line,
    };

    const options = {};
    if (shouldLog) {
      options.logValue = "displayName";
    }

    return dispatch(addBreakpoint(cx, breakpointLocation, options, disabled));
  };
}

export function removeBreakpointsAtLine(
  cx: Context,
  sourceId: string,
  line: number
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(removeBreakpoints(cx, breakpointsAtLine));
  };
}

export function disableBreakpointsAtLine(
  cx: Context,
  sourceId: string,
  line: number
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(toggleBreakpoints(cx, true, breakpointsAtLine));
  };
}

export function enableBreakpointsAtLine(
  cx: Context,
  sourceId: string,
  line: number
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(toggleBreakpoints(cx, false, breakpointsAtLine));
  };
}

export function toggleDisabledBreakpoint(cx: Context, breakpoint: Breakpoint) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (!breakpoint.disabled) {
      return dispatch(disableBreakpoint(cx, breakpoint));
    }
    return dispatch(enableBreakpoint(cx, breakpoint));
  };
}

export function enableXHRBreakpoint(index: number, bp?: XHRBreakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
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

export function disableXHRBreakpoint(index: number, bp?: XHRBreakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
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

export function updateXHRBreakpoint(
  index: number,
  path: string,
  method: string
) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
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
  return ({ dispatch, getState }: ThunkArgs) => {
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

export function setXHRBreakpoint(path: string, method: string) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoint = createXHRBreakpoint(path, method);

    return dispatch({
      type: "SET_XHR_BREAKPOINT",
      breakpoint,
      [PROMISE]: client.setXHRBreakpoint(path, method),
    });
  };
}

export function removeXHRBreakpoint(index: number) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
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
