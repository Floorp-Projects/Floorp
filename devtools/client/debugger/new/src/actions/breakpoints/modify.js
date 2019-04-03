/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "../types";
import type {
  Breakpoint,
  BreakpointOptions,
  SourceLocation
} from "../../types";

import {
  makeBreakpointLocation,
  makeBreakpointId,
  getASTLocation
} from "../../utils/breakpoint";

import { getTextAtPosition } from "../../utils/source";

import {
  getBreakpoint,
  getBreakpointPositionsForLocation,
  getFirstBreakpointPosition,
  getSourceFromId,
  getSymbols
} from "../../selectors";

import { loadSourceText } from "../sources/loadSourceText";
import { setBreakpointPositions } from "./breakpointPositions";

import { recordEvent } from "../../utils/telemetry";

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

function clientRemoveBreakpoint(breakpoint: Breakpoint) {
  return ({ getState, client }: ThunkArgs) => {
    const breakpointLocation = makeBreakpointLocation(
      getState(),
      breakpoint.generatedLocation
    );
    return client.removeBreakpoint(breakpointLocation);
  };
}

export function enableBreakpoint(initialBreakpoint: Breakpoint) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || !breakpoint.disabled) {
      return;
    }

    dispatch({
      type: "SET_BREAKPOINT",
      breakpoint: { ...breakpoint, disabled: false }
    });

    return dispatch(clientSetBreakpoint(breakpoint));
  };
}

export function addBreakpoint(
  initialLocation: SourceLocation,
  options: BreakpointOptions = {},
  disabled: boolean = false
) {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");

    const { sourceId, column } = initialLocation;

    await dispatch(setBreakpointPositions(sourceId));

    const position = column
      ? getBreakpointPositionsForLocation(getState(), initialLocation)
      : getFirstBreakpointPosition(getState(), initialLocation);

    if (!position) {
      return;
    }

    const { location, generatedLocation } = position;

    // Both the original and generated sources must be loaded to get the
    // breakpoint's text.
    await dispatch(
      loadSourceText(getSourceFromId(getState(), location.sourceId))
    );
    await dispatch(
      loadSourceText(getSourceFromId(getState(), generatedLocation.sourceId))
    );

    const source = getSourceFromId(getState(), location.sourceId);
    const generatedSource = getSourceFromId(
      getState(),
      generatedLocation.sourceId
    );

    const symbols = getSymbols(getState(), source);
    const astLocation = await getASTLocation(source, symbols, location);

    const originalText = getTextAtPosition(source, location);
    const text = getTextAtPosition(generatedSource, generatedLocation);

    const id = makeBreakpointId(location);
    const breakpoint = {
      id,
      disabled,
      options,
      location,
      astLocation,
      generatedLocation,
      text,
      originalText
    };

    // There cannot be multiple breakpoints with the same generated location.
    // Because a generated location cannot map to multiple original locations,
    // the only breakpoints that can map to this generated location have the
    // new breakpoint's |location| or |generatedLocation| as their own
    // |location|. We will overwrite any breakpoint at |location| with the
    // SET_BREAKPOINT action below, but need to manually remove any breakpoint
    // at |generatedLocation|.
    const generatedId = makeBreakpointId(breakpoint.generatedLocation);
    if (id != generatedId && getBreakpoint(getState(), generatedLocation)) {
      dispatch({
        type: "REMOVE_BREAKPOINT",
        location: generatedLocation
      });
    }

    dispatch({
      type: "SET_BREAKPOINT",
      breakpoint
    });

    if (disabled) {
      // If we just clobbered an enabled breakpoint with a disabled one, we need
      // to remove any installed breakpoint in the server.
      return dispatch(clientRemoveBreakpoint(breakpoint));
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
export function removeBreakpoint(initialBreakpoint: Breakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    recordEvent("remove_breakpoint");

    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint) {
      return;
    }

    dispatch({
      type: "REMOVE_BREAKPOINT",
      location: breakpoint.location
    });

    // If the breakpoint is disabled then it is not installed in the server.
    if (breakpoint.disabled) {
      return;
    }

    return dispatch(clientRemoveBreakpoint(breakpoint));
  };
}

/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpoint(initialBreakpoint: Breakpoint) {
  return ({ dispatch, getState, client }: ThunkArgs) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || breakpoint.disabled) {
      return;
    }

    dispatch({
      type: "SET_BREAKPOINT",
      breakpoint: { ...breakpoint, disabled: true }
    });

    return dispatch(clientRemoveBreakpoint(breakpoint));
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
  return ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    let breakpoint = getBreakpoint(getState(), location);
    if (!breakpoint) {
      return dispatch(addBreakpoint(location, options));
    }

    // Note: setting a breakpoint's options implicitly enables it.
    breakpoint = { ...breakpoint, disabled: false, options };

    dispatch({
      type: "SET_BREAKPOINT",
      breakpoint
    });

    return dispatch(clientSetBreakpoint(breakpoint));
  };
}
