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
  getConditionalPanelLocation,
  getBreakpointsForSource,
  isEmptyLineInSource,
  getBreakpointsAtLine
} from "../../selectors";
import { createXHRBreakpoint } from "../../utils/breakpoint";
import {
  addBreakpoint,
  removeBreakpoint,
  enableBreakpoint,
  disableBreakpoint
} from "./modify";
import remapLocations from "./remapLocations";
import { closeConditionalPanel } from "../ui";

// this will need to be changed so that addCLientBreakpoint is removed

import type { ThunkArgs } from "../types";
import type {
  Breakpoint,
  Source,
  SourceLocation,
  XHRBreakpoint
} from "../../types";

export * from "./breakpointPositions";
export * from "./modify";
export * from "./syncBreakpoint";

export function addHiddenBreakpoint(location: SourceLocation) {
  return ({ dispatch }: ThunkArgs) => {
    return dispatch(addBreakpoint(location, { hidden: true }));
  };
}

/**
 * Disable all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpointsInSource(source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
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
export function enableBreakpointsInSource(source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
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
export function toggleAllBreakpoints(shouldDisableBreakpoints: boolean) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
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
export function toggleBreakpoints(
  shouldDisableBreakpoints: boolean,
  breakpoints: Breakpoint[]
) {
  return async ({ dispatch }: ThunkArgs) => {
    const promises = breakpoints.map(
      breakpoint =>
        shouldDisableBreakpoints
          ? dispatch(disableBreakpoint(breakpoint))
          : dispatch(enableBreakpoint(breakpoint))
    );

    await Promise.all(promises);
  };
}

export function toggleBreakpointsAtLine(
  shouldDisableBreakpoints: boolean,
  line: number
) {
  return async ({ dispatch, getState }: ThunkArgs) => {
    const breakpoints = await getBreakpointsAtLine(getState(), line);
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
  return async ({ dispatch, getState }: ThunkArgs) => {
    const breakpointList = getBreakpointsList(getState());
    return Promise.all(
      breakpointList.map(bp => dispatch(removeBreakpoint(bp)))
    );
  };
}

/**
 * Removes breakpoints
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoints(breakpoints: Breakpoint[]) {
  return async ({ dispatch }: ThunkArgs) => {
    return Promise.all(breakpoints.map(bp => dispatch(removeBreakpoint(bp))));
  };
}

/**
 * Removes all breakpoints in a source
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpointsInSource(source: Source) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoints = getBreakpointsForSource(getState(), source.id);
    for (const breakpoint of breakpoints) {
      dispatch(removeBreakpoint(breakpoint));
    }
  };
}

export function remapBreakpoints(sourceId: string) {
  return async ({ dispatch, getState, sourceMaps }: ThunkArgs) => {
    const breakpoints = getBreakpointsList(getState());
    const newBreakpoints = await remapLocations(
      breakpoints,
      sourceId,
      sourceMaps
    );

    for (const bp of newBreakpoints) {
      await dispatch(addBreakpoint(bp.location, bp.options, bp.disabled));
    }
  };
}

export function toggleBreakpointAtLine(line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);

    if (!selectedSource) {
      return;
    }

    const bp = getBreakpointAtLocation(state, { line, column: undefined });
    const isEmptyLine = isEmptyLineInSource(state, line, selectedSource.id);

    if (!bp && isEmptyLine) {
      return;
    }

    if (getConditionalPanelLocation(getState())) {
      dispatch(closeConditionalPanel());
    }

    if (bp) {
      return dispatch(removeBreakpoint(bp));
    }
    return dispatch(
      addBreakpoint({
        sourceId: selectedSource.id,
        sourceUrl: selectedSource.url,
        line: line
      })
    );
  };
}

export function addBreakpointAtLine(line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const source = getSelectedSource(state);

    if (!source || isEmptyLineInSource(state, line, source.id)) {
      return;
    }

    return dispatch(
      addBreakpoint({
        sourceId: source.id,
        sourceUrl: source.url,
        column: undefined,
        line
      })
    );
  };
}

export function removeBreakpointsAtLine(sourceId: string, line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(removeBreakpoints(breakpointsAtLine));
  };
}

export function disableBreakpointsAtLine(sourceId: string, line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(toggleBreakpoints(true, breakpointsAtLine));
  };
}

export function enableBreakpointsAtLine(sourceId: string, line: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpointsAtLine = getBreakpointsForSource(
      getState(),
      sourceId,
      line
    );
    return dispatch(toggleBreakpoints(false, breakpointsAtLine));
  };
}

export function toggleDisabledBreakpoint(breakpoint: Breakpoint) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (!breakpoint.disabled) {
      return dispatch(disableBreakpoint(breakpoint));
    }
    return dispatch(enableBreakpoint(breakpoint));
  };
}

export function enableXHRBreakpoint(index: number, bp?: XHRBreakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = bp || xhrBreakpoints[index];
    const enabledBreakpoint = {
      ...breakpoint,
      disabled: false
    };

    return dispatch({
      type: "ENABLE_XHR_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      index,
      [PROMISE]: client.setXHRBreakpoint(breakpoint.path, breakpoint.method)
    });
  };
}

export function disableXHRBreakpoint(index: number, bp?: XHRBreakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const xhrBreakpoints = getXHRBreakpoints(getState());
    const breakpoint = bp || xhrBreakpoints[index];
    const disabledBreakpoint = {
      ...breakpoint,
      disabled: true
    };

    return dispatch({
      type: "DISABLE_XHR_BREAKPOINT",
      breakpoint: disabledBreakpoint,
      index,
      [PROMISE]: client.removeXHRBreakpoint(breakpoint.path, breakpoint.method)
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
      text: L10N.getFormatStr("xhrBreakpoints.item.label", path)
    };

    return dispatch({
      type: "UPDATE_XHR_BREAKPOINT",
      breakpoint: updatedBreakpoint,
      index,
      [PROMISE]: Promise.all([
        client.removeXHRBreakpoint(breakpoint.path, breakpoint.method),
        client.setXHRBreakpoint(path, method)
      ])
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
      [PROMISE]: client.setXHRBreakpoint(path, method)
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
      [PROMISE]: client.removeXHRBreakpoint(breakpoint.path, breakpoint.method)
    });
  };
}
