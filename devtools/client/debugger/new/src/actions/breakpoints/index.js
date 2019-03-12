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
  getBreakpoint,
  getBreakpointsList,
  getXHRBreakpoints,
  getSelectedSource,
  getBreakpointAtLocation,
  getConditionalPanelLocation,
  getBreakpointsForSource,
  isEmptyLineInSource
} from "../../selectors";
import {
  assertBreakpoint,
  createXHRBreakpoint,
  makeBreakpointLocation
} from "../../utils/breakpoint";
import {
  addBreakpoint,
  addHiddenBreakpoint,
  enableBreakpoint
} from "./addBreakpoint";
import remapLocations from "./remapLocations";
import { syncBreakpoint } from "./syncBreakpoint";
import { closeConditionalPanel } from "../ui";

// this will need to be changed so that addCLientBreakpoint is removed

import type { ThunkArgs, Action } from "../types";
import type {
  Breakpoint,
  BreakpointOptions,
  Source,
  SourceLocation,
  XHRBreakpoint
} from "../../types";

import { recordEvent } from "../../utils/telemetry";

export * from "./breakpointPositions";

async function removeBreakpointsPromise(client, state, breakpoint) {
  const breakpointLocation = makeBreakpointLocation(
    state,
    breakpoint.generatedLocation
  );
  await client.removeBreakpoint(breakpointLocation);
}

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoint(breakpoint: Breakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    if (breakpoint.loading) {
      return;
    }

    recordEvent("remove_breakpoint");

    // If the breakpoint is already disabled, we don't need to communicate
    // with the server. We just need to dispatch an action
    // simulating a successful server request
    if (breakpoint.disabled) {
      return dispatch(
        ({ type: "REMOVE_BREAKPOINT", breakpoint, status: "done" }: Action)
      );
    }

    return dispatch({
      type: "REMOVE_BREAKPOINT",
      breakpoint,
      disabled: false,
      [PROMISE]: removeBreakpointsPromise(client, getState(), breakpoint)
    });
  };
}

/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpoint(breakpoint: Breakpoint) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    if (breakpoint.loading) {
      return;
    }

    await removeBreakpointsPromise(client, getState(), breakpoint);

    const newBreakpoint: Breakpoint = { ...breakpoint, disabled: true };

    return dispatch(
      ({ type: "DISABLE_BREAKPOINT", breakpoint: newBreakpoint }: Action)
    );
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

    const modifiedBreakpoints = [];

    for (const breakpoint of breakpoints) {
      if (shouldDisableBreakpoints) {
        await removeBreakpointsPromise(client, getState(), breakpoint);
        const newBreakpoint: Breakpoint = { ...breakpoint, disabled: true };
        modifiedBreakpoints.push(newBreakpoint);
      } else {
        const newBreakpoint: Breakpoint = { ...breakpoint, disabled: false };
        modifiedBreakpoints.push(newBreakpoint);
      }
    }

    if (shouldDisableBreakpoints) {
      return dispatch(
        ({
          type: "DISABLE_ALL_BREAKPOINTS",
          breakpoints: modifiedBreakpoints
        }: Action)
      );
    }

    return dispatch(
      ({
        type: "ENABLE_ALL_BREAKPOINTS",
        breakpoints: modifiedBreakpoints
      }: Action)
    );
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

    return dispatch(
      ({
        type: "REMAP_BREAKPOINTS",
        breakpoints: newBreakpoints
      }: Action)
    );
  };
}

/**
 * Update the options of a breakpoint.
 *
 * @throws {Error} "not implemented"
 * @memberof actions/breakpoints
 * @static
 * @param {SourceLocation} location
 *        @see DebuggerController.Breakpoints.addBreakpoint
 * @param {Object} options
 *        Any options to set on the breakpoint
 */
export function setBreakpointOptions(
  location: SourceLocation,
  options: BreakpointOptions = {}
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const bp = getBreakpoint(getState(), location);
    if (!bp) {
      return dispatch(addBreakpoint(location, options));
    }

    if (bp.loading) {
      return;
    }

    if (bp.disabled) {
      await dispatch(enableBreakpoint(bp));
    }

    const breakpointLocation = makeBreakpointLocation(
      getState(),
      bp.generatedLocation
    );
    await client.setBreakpoint(breakpointLocation, options);

    const newBreakpoint = { ...bp, disabled: false, options };

    assertBreakpoint(newBreakpoint);

    return dispatch(
      ({
        type: "SET_BREAKPOINT_OPTIONS",
        breakpoint: newBreakpoint
      }: Action)
    );
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

    if ((!bp && isEmptyLine) || (bp && bp.loading)) {
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
    if (breakpoint.loading) {
      return;
    }

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

export { addBreakpoint, addHiddenBreakpoint, enableBreakpoint, syncBreakpoint };
