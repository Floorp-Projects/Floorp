/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  makeBreakpointLocation,
  makeBreakpointId,
  getASTLocation,
} from "../../utils/breakpoint";

import {
  getBreakpoint,
  getBreakpointPositionsForLocation,
  getFirstBreakpointPosition,
  getSymbols,
  getSource,
  getSourceContent,
  getBreakpointsList,
  getPendingBreakpointList,
} from "../../selectors";

import { setBreakpointPositions } from "./breakpointPositions";

import { recordEvent } from "../../utils/telemetry";
import { comparePosition } from "../../utils/location";
import { getTextAtPosition } from "../../utils/source";

import type { ThunkArgs } from "../types";
import type {
  Breakpoint,
  BreakpointOptions,
  BreakpointPosition,
  SourceLocation,
  Context,
} from "../../types";

// This file has the primitive operations used to modify individual breakpoints
// and keep them in sync with the breakpoints installed on server threads. These
// are collected here to make it easier to preserve the following invariant:
//
// Breakpoints are included in reducer state iff they are disabled or requests
// have been dispatched to set them in all server threads.
//
// To maintain this property, updates to the reducer and installed breakpoints
// must happen with no intervening await. Using await allows other operations to
// modify the breakpoint state in the interim and potentially cause breakpoint
// state to go out of sync.
//
// The reducer is optimistically updated when users set or remove a breakpoint,
// but it might take a little while before the breakpoints have been set or
// removed in each thread. Once all outstanding requests sent to a thread have
// been processed, the reducer and server threads will be in sync.
//
// There is another exception to the above invariant when first connecting to
// the server: breakpoints have been installed on all generated locations in the
// pending breakpoints, but no breakpoints have been added to the reducer. When
// a matching source appears, either the server breakpoint will be removed or a
// breakpoint will be added to the reducer, to restore the above invariant.
// See syncBreakpoint.js for more.

function clientSetBreakpoint(breakpoint: Breakpoint) {
  return ({ getState, client }: ThunkArgs) => {
    const breakpointLocation = makeBreakpointLocation(
      getState(),
      breakpoint.generatedLocation
    );
    return client.setBreakpoint(breakpointLocation, breakpoint.options);
  };
}

function clientRemoveBreakpoint(generatedLocation: SourceLocation) {
  return ({ getState, client }: ThunkArgs) => {
    const breakpointLocation = makeBreakpointLocation(
      getState(),
      generatedLocation
    );
    return client.removeBreakpoint(breakpointLocation);
  };
}

export function enableBreakpoint(cx: Context, initialBreakpoint: Breakpoint) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || !breakpoint.disabled) {
      return;
    }

    dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint: { ...breakpoint, disabled: false },
    });

    return dispatch(clientSetBreakpoint(breakpoint));
  };
}

export function addBreakpoint(
  cx: Context,
  initialLocation: SourceLocation,
  options: BreakpointOptions = {},
  disabled: boolean = false,
  shouldCancel: () => boolean = () => false
) {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");

    const { sourceId, column, line } = initialLocation;

    await dispatch(setBreakpointPositions({ cx, sourceId, line }));

    const position: ?BreakpointPosition = column
      ? getBreakpointPositionsForLocation(getState(), initialLocation)
      : getFirstBreakpointPosition(getState(), initialLocation);

    if (!position) {
      return;
    }

    const { location, generatedLocation } = position;

    const source = getSource(getState(), location.sourceId);
    const generatedSource = getSource(getState(), generatedLocation.sourceId);

    if (!source || !generatedSource) {
      return;
    }

    const symbols = getSymbols(getState(), source);
    const astLocation = getASTLocation(source, symbols, location);

    const originalContent = getSourceContent(getState(), source.id);
    const originalText = getTextAtPosition(
      source.id,
      originalContent,
      location
    );

    const content = getSourceContent(getState(), generatedSource.id);
    const text = getTextAtPosition(
      generatedSource.id,
      content,
      generatedLocation
    );

    const id = makeBreakpointId(location);
    const breakpoint = {
      id,
      disabled,
      options,
      location,
      astLocation,
      generatedLocation,
      text,
      originalText,
    };

    if (shouldCancel()) {
      return;
    }

    dispatch({ type: "SET_BREAKPOINT", cx, breakpoint });

    if (disabled) {
      // If we just clobbered an enabled breakpoint with a disabled one, we need
      // to remove any installed breakpoint in the server.
      return dispatch(clientRemoveBreakpoint(generatedLocation));
    }

    return dispatch(clientSetBreakpoint(breakpoint));
  };
}

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoint(cx: Context, initialBreakpoint: Breakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    recordEvent("remove_breakpoint");

    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint) {
      return;
    }

    dispatch({
      type: "REMOVE_BREAKPOINT",
      cx,
      location: breakpoint.location,
    });

    // If the breakpoint is disabled then it is not installed in the server.
    if (breakpoint.disabled) {
      return;
    }

    return dispatch(clientRemoveBreakpoint(breakpoint.generatedLocation));
  };
}

/**
 * Remove all installed, pending, and client breakpoints associated with a
 * target generated location.
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpointAtGeneratedLocation(
  cx: Context,
  target: SourceLocation
) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    // Remove any breakpoints matching the generated location.
    const breakpoints = getBreakpointsList(getState());
    for (const { location, generatedLocation } of breakpoints) {
      if (
        generatedLocation.sourceId == target.sourceId &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_BREAKPOINT",
          cx,
          location,
        });
      }
    }

    // Remove any remaining pending breakpoints matching the generated location.
    const pending = getPendingBreakpointList(getState());
    for (const { location, generatedLocation } of pending) {
      if (
        generatedLocation.sourceUrl == target.sourceUrl &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_PENDING_BREAKPOINT",
          cx,
          location,
        });
      }
    }

    // Remove the breakpoint from the client itself.
    return dispatch(clientRemoveBreakpoint(target));
  };
}

/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpoint(cx: Context, initialBreakpoint: Breakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || breakpoint.disabled) {
      return;
    }

    dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint: { ...breakpoint, disabled: true },
    });

    return dispatch(clientRemoveBreakpoint(breakpoint.generatedLocation));
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
  cx: Context,
  location: SourceLocation,
  options: BreakpointOptions = {}
) {
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    let breakpoint = getBreakpoint(getState(), location);
    if (!breakpoint) {
      return dispatch(addBreakpoint(cx, location, options));
    }

    // Note: setting a breakpoint's options implicitly enables it.
    breakpoint = { ...breakpoint, disabled: false, options };

    dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint,
    });

    return dispatch(clientSetBreakpoint(breakpoint));
  };
}
