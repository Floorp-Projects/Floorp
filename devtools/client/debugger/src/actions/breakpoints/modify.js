/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createBreakpoint } from "../../client/firefox/create";
import {
  makeBreakpointLocation,
  makeBreakpointId,
} from "../../utils/breakpoint";
import {
  getBreakpoint,
  getBreakpointPositionsForLocation,
  getFirstBreakpointPosition,
  getLocationSource,
  getSourceContent,
  getBreakpointsList,
  getPendingBreakpointList,
  isMapScopesEnabled,
} from "../../selectors";

import { setBreakpointPositions } from "./breakpointPositions";
import { setSkipPausing } from "../pause/skipPausing";

import { PROMISE } from "../utils/middleware/promise";
import { recordEvent } from "../../utils/telemetry";
import { comparePosition } from "../../utils/location";
import { getTextAtPosition } from "../../utils/source";
import { getMappedScopesForLocation } from "../pause/mapScopes";
import { validateNavigateContext } from "../../utils/context";

// This file has the primitive operations used to modify individual breakpoints
// and keep them in sync with the breakpoints installed on server threads. These
// are collected here to make it easier to preserve the following invariant:
//
// Breakpoints are included in reducer state if they are disabled or requests
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

async function clientSetBreakpoint(
  client,
  cx,
  { getState, dispatch },
  breakpoint
) {
  const breakpointLocation = makeBreakpointLocation(
    getState(),
    breakpoint.generatedLocation
  );
  const shouldMapBreakpointExpressions =
    isMapScopesEnabled(getState()) &&
    getLocationSource(getState(), breakpoint.location).isOriginal &&
    (breakpoint.options.logValue || breakpoint.options.condition);

  if (shouldMapBreakpointExpressions) {
    breakpoint = await dispatch(updateBreakpointSourceMapping(cx, breakpoint));
  }
  return client.setBreakpoint(breakpointLocation, breakpoint.options);
}

function clientRemoveBreakpoint(client, state, generatedLocation) {
  const breakpointLocation = makeBreakpointLocation(state, generatedLocation);
  return client.removeBreakpoint(breakpointLocation);
}

export function enableBreakpoint(cx, initialBreakpoint) {
  return thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || !breakpoint.disabled) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint: createBreakpoint({ ...breakpoint, disabled: false }),
      [PROMISE]: clientSetBreakpoint(client, cx, thunkArgs, breakpoint),
    });
  };
}

export function addBreakpoint(
  cx,
  initialLocation,
  options = {},
  disabled,
  shouldCancel = () => false
) {
  return async thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    recordEvent("add_breakpoint");

    const { column, line } = initialLocation;
    const initialSource = getLocationSource(getState(), initialLocation);

    await dispatch(
      setBreakpointPositions({ cx, sourceId: initialSource.id, line })
    );

    const position = column
      ? getBreakpointPositionsForLocation(getState(), initialLocation)
      : getFirstBreakpointPosition(getState(), initialLocation);

    // No position is found if the `initialLocation` is on a non-breakable line or
    // the line no longer exists.
    if (!position) {
      return null;
    }

    const { location, generatedLocation } = position;

    const source = getLocationSource(getState(), location);
    const generatedSource = getLocationSource(getState(), generatedLocation);

    if (!source || !generatedSource) {
      return null;
    }

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
    const breakpoint = createBreakpoint({
      id,
      thread: generatedSource.thread,
      disabled,
      options,
      location,
      generatedLocation,
      text,
      originalText,
    });

    if (shouldCancel()) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint,
      // If we just clobbered an enabled breakpoint with a disabled one, we need
      // to remove any installed breakpoint in the server.
      [PROMISE]: disabled
        ? clientRemoveBreakpoint(client, getState(), generatedLocation)
        : clientSetBreakpoint(client, cx, thunkArgs, breakpoint),
    });
  };
}

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoint(cx, initialBreakpoint) {
  return ({ dispatch, getState, client }) => {
    recordEvent("remove_breakpoint");

    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "REMOVE_BREAKPOINT",
      cx,
      breakpoint,
      // If the breakpoint is disabled then it is not installed in the server.
      [PROMISE]: breakpoint.disabled
        ? Promise.resolve()
        : clientRemoveBreakpoint(
            client,
            getState(),
            breakpoint.generatedLocation
          ),
    });
  };
}

/**
 * Remove all installed, pending, and client breakpoints associated with a
 * target generated location.
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpointAtGeneratedLocation(cx, target) {
  return ({ dispatch, getState, client }) => {
    // remove breakpoint from the server
    const onBreakpointRemoved = clientRemoveBreakpoint(
      client,
      getState(),
      target
    );
    // Remove any breakpoints matching the generated location.
    const breakpoints = getBreakpointsList(getState());
    for (const breakpoint of breakpoints) {
      const { generatedLocation } = breakpoint;
      if (
        generatedLocation.sourceId == target.sourceId &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_BREAKPOINT",
          cx,
          breakpoint,
          [PROMISE]: onBreakpointRemoved,
        });
      }
    }

    // Remove any remaining pending breakpoints matching the generated location.
    const pending = getPendingBreakpointList(getState());
    for (const breakpoint of pending) {
      const { generatedLocation } = breakpoint;
      if (
        generatedLocation.sourceUrl == target.sourceUrl &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_PENDING_BREAKPOINT",
          cx,
          breakpoint,
        });
      }
    }
    return onBreakpointRemoved;
  };
}

/**
 * Disable a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function disableBreakpoint(cx, initialBreakpoint) {
  return ({ dispatch, getState, client }) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || breakpoint.disabled) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint: createBreakpoint({ ...breakpoint, disabled: true }),
      [PROMISE]: clientRemoveBreakpoint(
        client,
        getState(),
        breakpoint.generatedLocation
      ),
    });
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
export function setBreakpointOptions(cx, location, options = {}) {
  return thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    let breakpoint = getBreakpoint(getState(), location);
    if (!breakpoint) {
      return dispatch(addBreakpoint(cx, location, options));
    }

    // Note: setting a breakpoint's options implicitly enables it.
    breakpoint = createBreakpoint({ ...breakpoint, disabled: false, options });

    return dispatch({
      type: "SET_BREAKPOINT",
      cx,
      breakpoint,
      [PROMISE]: clientSetBreakpoint(client, cx, thunkArgs, breakpoint),
    });
  };
}

async function updateExpression(
  evaluationsParser,
  mappings,
  originalExpression
) {
  const mapped = await evaluationsParser.mapExpression(
    originalExpression,
    mappings,
    [],
    false,
    false
  );
  if (!mapped) {
    return originalExpression;
  }
  if (!originalExpression.trimEnd().endsWith(";")) {
    return mapped.expression.replace(/;$/, "");
  }
  return mapped.expression;
}

function updateBreakpointSourceMapping(cx, breakpoint) {
  return async ({ getState, dispatch, evaluationsParser }) => {
    const options = { ...breakpoint.options };

    const mappedScopes = await dispatch(
      getMappedScopesForLocation(breakpoint.location)
    );
    if (!mappedScopes) {
      return breakpoint;
    }
    const { mappings } = mappedScopes;

    if (options.condition) {
      options.condition = await updateExpression(
        evaluationsParser,
        mappings,
        options.condition
      );
    }
    if (options.logValue) {
      options.logValue = await updateExpression(
        evaluationsParser,
        mappings,
        options.logValue
      );
    }

    validateNavigateContext(getState(), cx);
    return { ...breakpoint, options };
  };
}
