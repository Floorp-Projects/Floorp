/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createBreakpoint } from "../../client/firefox/create";
import {
  makeBreakpointServerLocation,
  makeBreakpointId,
} from "../../utils/breakpoint";
import {
  getBreakpoint,
  getBreakpointPositionsForLocation,
  getFirstBreakpointPosition,
  getSettledSourceTextContent,
  getBreakpointsList,
  getPendingBreakpointList,
  isMapScopesEnabled,
  getBlackBoxRanges,
  isSourceMapIgnoreListEnabled,
  isSourceOnSourceMapIgnoreList,
} from "../../selectors";

import { setBreakpointPositions } from "./breakpointPositions";
import { setSkipPausing } from "../pause/skipPausing";

import { PROMISE } from "../utils/middleware/promise";
import { recordEvent } from "../../utils/telemetry";
import { comparePosition } from "../../utils/location";
import { getTextAtPosition, isLineBlackboxed } from "../../utils/source";
import { getMappedScopesForLocation } from "../pause/mapScopes";
import { validateBreakpoint } from "../../utils/context";

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

async function clientSetBreakpoint(client, { getState, dispatch }, breakpoint) {
  const breakpointServerLocation = makeBreakpointServerLocation(
    getState(),
    breakpoint.generatedLocation
  );
  const shouldMapBreakpointExpressions =
    isMapScopesEnabled(getState()) &&
    breakpoint.location.source.isOriginal &&
    (breakpoint.options.logValue || breakpoint.options.condition);

  if (shouldMapBreakpointExpressions) {
    breakpoint = await dispatch(updateBreakpointSourceMapping(breakpoint));
  }
  return client.setBreakpoint(breakpointServerLocation, breakpoint.options);
}

function clientRemoveBreakpoint(client, state, generatedLocation) {
  const breakpointServerLocation = makeBreakpointServerLocation(
    state,
    generatedLocation
  );
  return client.removeBreakpoint(breakpointServerLocation);
}

export function enableBreakpoint(initialBreakpoint) {
  return thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    const state = getState();
    const breakpoint = getBreakpoint(state, initialBreakpoint.location);
    const blackboxedRanges = getBlackBoxRanges(state);
    const isSourceOnIgnoreList =
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, breakpoint.location.source);
    if (
      !breakpoint ||
      !breakpoint.disabled ||
      isLineBlackboxed(
        blackboxedRanges[breakpoint.location.source.url],
        breakpoint.location.line,
        isSourceOnIgnoreList
      )
    ) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "SET_BREAKPOINT",
      breakpoint: createBreakpoint({ ...breakpoint, disabled: false }),
      [PROMISE]: clientSetBreakpoint(client, thunkArgs, breakpoint),
    });
  };
}

export function addBreakpoint(
  initialLocation,
  options = {},
  disabled,
  shouldCancel = () => false
) {
  return async thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    recordEvent("add_breakpoint");

    await dispatch(setBreakpointPositions(initialLocation));

    const position = initialLocation.column
      ? getBreakpointPositionsForLocation(getState(), initialLocation)
      : getFirstBreakpointPosition(getState(), initialLocation);

    // No position is found if the `initialLocation` is on a non-breakable line or
    // the line no longer exists.
    if (!position) {
      return null;
    }

    const { location, generatedLocation } = position;

    if (!location.source || !generatedLocation.source) {
      return null;
    }

    const originalContent = getSettledSourceTextContent(getState(), location);
    const originalText = getTextAtPosition(
      location.source.id,
      originalContent,
      location
    );

    const content = getSettledSourceTextContent(getState(), generatedLocation);
    const text = getTextAtPosition(
      generatedLocation.source.id,
      content,
      generatedLocation
    );

    const id = makeBreakpointId(location);
    const breakpoint = createBreakpoint({
      id,
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
      breakpoint,
      // If we just clobbered an enabled breakpoint with a disabled one, we need
      // to remove any installed breakpoint in the server.
      [PROMISE]: disabled
        ? clientRemoveBreakpoint(client, getState(), generatedLocation)
        : clientSetBreakpoint(client, thunkArgs, breakpoint),
    });
  };
}

/**
 * Remove a single breakpoint
 *
 * @memberof actions/breakpoints
 * @static
 */
export function removeBreakpoint(initialBreakpoint) {
  return ({ dispatch, getState, client }) => {
    recordEvent("remove_breakpoint");

    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "REMOVE_BREAKPOINT",
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
 * @param {Object} target
 *        Location object where to remove breakpoints.
 */
export function removeBreakpointAtGeneratedLocation(target) {
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
        generatedLocation.source.id == target.source.id &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_BREAKPOINT",
          breakpoint,
          [PROMISE]: onBreakpointRemoved,
        });
      }
    }

    // Remove any remaining pending breakpoints matching the generated location.
    const pending = getPendingBreakpointList(getState());
    for (const pendingBreakpoint of pending) {
      const { generatedLocation } = pendingBreakpoint;
      if (
        generatedLocation.sourceUrl == target.source.url &&
        comparePosition(generatedLocation, target)
      ) {
        dispatch({
          type: "REMOVE_PENDING_BREAKPOINT",
          pendingBreakpoint,
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
export function disableBreakpoint(initialBreakpoint) {
  return ({ dispatch, getState, client }) => {
    const breakpoint = getBreakpoint(getState(), initialBreakpoint.location);
    if (!breakpoint || breakpoint.disabled) {
      return null;
    }

    dispatch(setSkipPausing(false));
    return dispatch({
      type: "SET_BREAKPOINT",
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
export function setBreakpointOptions(location, options = {}) {
  return thunkArgs => {
    const { dispatch, getState, client } = thunkArgs;
    let breakpoint = getBreakpoint(getState(), location);
    if (!breakpoint) {
      return dispatch(addBreakpoint(location, options));
    }

    // Note: setting a breakpoint's options implicitly enables it.
    breakpoint = createBreakpoint({ ...breakpoint, disabled: false, options });

    return dispatch({
      type: "SET_BREAKPOINT",
      breakpoint,
      [PROMISE]: clientSetBreakpoint(client, thunkArgs, breakpoint),
    });
  };
}

async function updateExpression(parserWorker, mappings, originalExpression) {
  const mapped = await parserWorker.mapExpression(
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

function updateBreakpointSourceMapping(breakpoint) {
  return async ({ getState, dispatch, parserWorker }) => {
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
        parserWorker,
        mappings,
        options.condition
      );
    }
    if (options.logValue) {
      options.logValue = await updateExpression(
        parserWorker,
        mappings,
        options.logValue
      );
    }

    // As we waited for lots of asynchronous operations,
    // verify that the breakpoint is still valid before
    // trying to set/update it on the server.
    validateBreakpoint(getState(), breakpoint);

    return { ...breakpoint, options };
  };
}
