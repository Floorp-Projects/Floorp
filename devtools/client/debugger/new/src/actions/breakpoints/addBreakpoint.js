/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isOriginalId } from "devtools-source-map";
import {
  locationMoved,
  breakpointExists,
  assertBreakpoint,
  createBreakpoint,
  getASTLocation,
  assertLocation
} from "../../utils/breakpoint";
import { PROMISE } from "../utils/middleware/promise";
import { getSource, getSymbols, getBreakpoint } from "../../selectors";
import { getGeneratedLocation } from "../../utils/source-maps";
import { getTextAtPosition } from "../../utils/source";
import { recordEvent } from "../../utils/telemetry";

async function addBreakpointPromise(getState, client, sourceMaps, breakpoint) {
  const state = getState();
  const source = getSource(state, breakpoint.location.sourceId);

  const location = {
    ...breakpoint.location,
    sourceId: source.id,
    sourceUrl: source.url
  };

  const generatedLocation = await getGeneratedLocation(
    state,
    source,
    location,
    sourceMaps
  );

  const generatedSource = getSource(state, generatedLocation.sourceId);

  assertLocation(location);
  assertLocation(generatedLocation);

  if (breakpointExists(state, location)) {
    const newBreakpoint = { ...breakpoint, location, generatedLocation };
    assertBreakpoint(newBreakpoint);
    return { breakpoint: newBreakpoint };
  }

  const { id, actualLocation } = await client.setBreakpoint(
    generatedLocation,
    breakpoint.condition,
    isOriginalId(location.sourceId)
  );

  const newGeneratedLocation = actualLocation || generatedLocation;
  const newLocation = await sourceMaps.getOriginalLocation(
    newGeneratedLocation
  );

  const symbols = getSymbols(getState(), source);
  const astLocation = await getASTLocation(source, symbols, newLocation);

  const originalText = getTextAtPosition(source, location);
  const text = getTextAtPosition(generatedSource, actualLocation);

  const newBreakpoint = {
    id,
    disabled: false,
    hidden: breakpoint.hidden,
    loading: false,
    condition: breakpoint.condition,
    location: newLocation,
    astLocation,
    generatedLocation: newGeneratedLocation,
    text,
    originalText
  };

  assertBreakpoint(newBreakpoint);

  const previousLocation = locationMoved(location, newLocation)
    ? location
    : null;

  return {
    breakpoint: newBreakpoint,
    previousLocation
  };
}

/**
 * Add a new hidden breakpoint
 *
 * @memberOf actions/breakpoints
 * @param location
 * @return {function(ThunkArgs)}
 */
export function addHiddenBreakpoint(location: Location) {
  return ({ dispatch }: ThunkArgs) => {
    return dispatch(addBreakpoint(location, { hidden: true }));
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
export function enableBreakpoint(location: Location) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    const breakpoint = getBreakpoint(getState(), location);
    if (!breakpoint || breakpoint.loading) {
      return;
    }

    // To instantly reflect in the UI, we optimistically enable the breakpoint
    const enabledBreakpoint = {
      ...breakpoint,
      disabled: false
    };

    return dispatch({
      type: "ENABLE_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
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

export function addBreakpoint(
  location: Location,
  { condition, hidden }: addBreakpointOptions = {}
) {
  const breakpoint = createBreakpoint(location, { condition, hidden });
  return ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");

    return dispatch({
      type: "ADD_BREAKPOINT",
      breakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}
