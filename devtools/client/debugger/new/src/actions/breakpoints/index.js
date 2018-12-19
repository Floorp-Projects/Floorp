/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Redux actions for breakpoints
 * @module actions/breakpoints
 */

import { isOriginalId } from "devtools-source-map";
import { PROMISE } from "../utils/middleware/promise";
import {
  getBreakpoint,
  getBreakpointsList,
  getXHRBreakpoints,
  getSelectedSource,
  getBreakpointAtLocation,
  getBreakpointsAtLine
} from "../../selectors";
import { assertBreakpoint, createXHRBreakpoint } from "../../utils/breakpoint";
import {
  addBreakpoint,
  addHiddenBreakpoint,
  enableBreakpoint
} from "./addBreakpoint";
import remapLocations from "./remapLocations";
import { syncBreakpoint } from "./syncBreakpoint";
import { isEmptyLineInSource } from "../../reducers/ast";

// this will need to be changed so that addCLientBreakpoint is removed

import type { ThunkArgs, Action } from "../types";
import type { Breakpoint, SourceLocation, XHRBreakpoint } from "../../types";

import { recordEvent } from "../../utils/telemetry";

export type addBreakpointOptions = {
  condition?: string,
  hidden?: boolean
};

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoint(location: SourceLocation) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const bp = getBreakpoint(getState(), location);
    if (!bp || bp.loading) {
      return;
    }

    recordEvent("remove_breakpoint");

    // If the breakpoint is already disabled, we don't need to communicate
    // with the server. We just need to dispatch an action
    // simulating a successful server request
    if (bp.disabled) {
      return dispatch(
        ({
          type: "REMOVE_BREAKPOINT",
          breakpoint: bp,
          status: "done"
        }: Action)
      );
    }

    return dispatch({
      type: "REMOVE_BREAKPOINT",
      breakpoint: bp,
      disabled: false,
      [PROMISE]: client.removeBreakpoint(bp.generatedLocation)
    });
  };
}

/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpoint(location: SourceLocation) {
  return async ({ dispatch, getState, client }: ThunkArgs) => {
    const bp = getBreakpoint(getState(), location);

    if (!bp || bp.loading) {
      return;
    }

    await client.removeBreakpoint(bp.generatedLocation);
    const newBreakpoint: Breakpoint = { ...bp, disabled: true };

    return dispatch(
      ({
        type: "DISABLE_BREAKPOINT",
        breakpoint: newBreakpoint
      }: Action)
    );
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
        await client.removeBreakpoint(breakpoint.generatedLocation);
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
    const promises = breakpoints.map(breakpoint =>
      shouldDisableBreakpoints
        ? dispatch(disableBreakpoint(breakpoint.location))
        : dispatch(enableBreakpoint(breakpoint.location))
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
      breakpointList.map(bp => dispatch(removeBreakpoint(bp.location)))
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
    return Promise.all(
      breakpoints.map(bp => dispatch(removeBreakpoint(bp.location)))
    );
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
 * Update the condition of a breakpoint.
 *
 * @throws {Error} "not implemented"
 * @memberof actions/breakpoints
 * @static
 * @param {SourceLocation} location
 *        @see DebuggerController.Breakpoints.addBreakpoint
 * @param {string} condition
 *        The condition to set on the breakpoint
 * @param {Boolean} $1.disabled Disable value for breakpoint value
 */
export function setBreakpointCondition(
  location: SourceLocation,
  { condition }: addBreakpointOptions = {}
) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const bp = getBreakpoint(getState(), location);
    if (!bp) {
      return dispatch(addBreakpoint(location, { condition }));
    }

    if (bp.loading) {
      return;
    }

    if (bp.disabled) {
      await dispatch(enableBreakpoint(location));
    }

    await client.setBreakpointCondition(
      bp.id,
      location,
      condition,
      isOriginalId(bp.location.sourceId)
    );

    const newBreakpoint = { ...bp, disabled: false, condition };

    assertBreakpoint(newBreakpoint);

    return dispatch(
      ({
        type: "SET_BREAKPOINT_CONDITION",
        breakpoint: newBreakpoint
      }: Action)
    );
  };
}

export function toggleBreakpoint(line: number, column?: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);

    if (!selectedSource) {
      return;
    }

    const bp = getBreakpointAtLocation(state, { line, column });
    const isEmptyLine = isEmptyLineInSource(state, line, selectedSource.id);

    if ((!bp && isEmptyLine) || (bp && bp.loading)) {
      return;
    }

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      return dispatch(
        removeBreakpoint({
          sourceId: bp.location.sourceId,
          sourceUrl: bp.location.sourceUrl,
          line: bp.location.line,
          column: column || bp.location.column
        })
      );
    }
    return dispatch(
      addBreakpoint({
        sourceId: selectedSource.id,
        sourceUrl: selectedSource.url,
        line: line,
        column: column
      })
    );
  };
}

export function toggleBreakpointsAtLine(line: number, column?: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const state = getState();
    const selectedSource = getSelectedSource(state);

    if (!selectedSource) {
      return;
    }

    const bps = getBreakpointsAtLine(state, line);
    const isEmptyLine = isEmptyLineInSource(state, line, selectedSource.id);

    if (bps.length === 0 && !isEmptyLine) {
      return dispatch(
        addBreakpoint({
          sourceId: selectedSource.id,
          sourceUrl: selectedSource.url,
          line,
          column
        })
      );
    }

    return Promise.all(bps.map(bp => dispatch(removeBreakpoint(bp.location))));
  };
}

export function addOrToggleDisabledBreakpoint(line: number, column?: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const selectedSource = getSelectedSource(getState());

    if (!selectedSource) {
      return;
    }

    const bp = getBreakpointAtLocation(getState(), { line, column });

    if (bp && bp.loading) {
      return;
    }

    if (bp) {
      // NOTE: it's possible the breakpoint has slid to a column
      return dispatch(
        toggleDisabledBreakpoint(line, column || bp.location.column)
      );
    }

    return dispatch(
      addBreakpoint({
        sourceId: selectedSource.id,
        sourceUrl: selectedSource.url,
        line: line,
        column: column
      })
    );
  };
}

export function toggleDisabledBreakpoint(line: number, column?: number) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const bp = getBreakpointAtLocation(getState(), { line, column });
    if (!bp || bp.loading) {
      return;
    }

    if (!bp.disabled) {
      return dispatch(disableBreakpoint(bp.location));
    }
    return dispatch(enableBreakpoint(bp.location));
  };
}

export function enableXHRBreakpoint(index: number, bp: XHRBreakpoint) {
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

export function disableXHRBreakpoint(index: number, bp: XHRBreakpoint) {
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
      text: `URL contains "${path}"`
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
