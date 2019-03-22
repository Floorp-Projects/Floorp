/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  assertBreakpoint,
  createBreakpoint,
  getASTLocation,
  makeBreakpointId,
  makeBreakpointLocation,
  findPosition
} from "../../utils/breakpoint";
import { PROMISE } from "../utils/middleware/promise";
import {
  getSymbols,
  getFirstBreakpointPosition,
  getBreakpointPositionsForSource,
  getSourceFromId
} from "../../selectors";

import { getTextAtPosition } from "../../utils/source";
import { recordEvent } from "../../utils/telemetry";

import type {
  BreakpointOptions,
  Breakpoint,
  SourceLocation
} from "../../types";
import type { ThunkArgs } from "../types";

async function addBreakpointPromise(getState, client, sourceMaps, breakpoint) {
  const state = getState();
  const { location, generatedLocation } = breakpoint;
  const source = getSourceFromId(state, location.sourceId);
  const generatedSource = getSourceFromId(state, generatedLocation.sourceId);

  const breakpointLocation = makeBreakpointLocation(
    getState(),
    generatedLocation
  );
  await client.setBreakpoint(breakpointLocation, breakpoint.options);

  const symbols = getSymbols(getState(), source);
  const astLocation = await getASTLocation(source, symbols, location);

  const originalText = getTextAtPosition(source, location);
  const text = getTextAtPosition(generatedSource, generatedLocation);

  const newBreakpoint = {
    id: makeBreakpointId(generatedLocation),
    disabled: false,
    loading: false,
    options: breakpoint.options,
    location,
    astLocation,
    generatedLocation,
    text,
    originalText
  };

  assertBreakpoint(newBreakpoint);

  return newBreakpoint;
}

export function addHiddenBreakpoint(location: SourceLocation) {
  return ({ dispatch }: ThunkArgs) => {
    return dispatch(addBreakpoint(location, { hidden: true }));
  };
}

export function enableBreakpoint(breakpoint: Breakpoint) {
  return async ({ dispatch, getState, client, sourceMaps }: ThunkArgs) => {
    if (breakpoint.loading) {
      return;
    }

    // To instantly reflect in the UI, we optimistically enable the breakpoint
    const enabledBreakpoint = { ...breakpoint, disabled: false };

    return dispatch({
      type: "ENABLE_BREAKPOINT",
      breakpoint: enabledBreakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}

export function addBreakpoint(
  location: SourceLocation,
  options: BreakpointOptions = {}
) {
  return async ({ dispatch, getState, sourceMaps, client }: ThunkArgs) => {
    recordEvent("add_breakpoint");
    let position;
    const { sourceId, column } = location;

    if (column === undefined) {
      position = getFirstBreakpointPosition(getState(), location);
    } else {
      const positions = getBreakpointPositionsForSource(getState(), sourceId);
      position = findPosition(positions, location);
    }

    if (!position) {
      return;
    }

    const breakpoint = createBreakpoint(position, { options });

    return dispatch({
      type: "ADD_BREAKPOINT",
      breakpoint,
      [PROMISE]: addBreakpointPromise(getState, client, sourceMaps, breakpoint)
    });
  };
}
